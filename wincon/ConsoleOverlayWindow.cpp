#include "stdafx.h"
#include "ConsoleOverlayWindow.h"

#include <algorithm>
#include <Shellapi.h>
#include <windowsx.h>


ConsoleOverlayWindow::ConsoleOverlayWindow() :
	_selectionHelper(_consoleHelper),
	_selectionView(_consoleHelper, _selectionHelper),
	_selectionOperation(_consoleHelper, _selectionHelper),
	_resizeOperation(_consoleHelper)
{}

ConsoleOverlayWindow::~ConsoleOverlayWindow()
{
	::DestroyWindow(_hWndOverlay);
}

bool ConsoleOverlayWindow::Create(DWORD consoleWindowThreadId)
{
	::WNDCLASSEX wcex{};
	wcex.cbSize = sizeof(wcex);
	wcex.hInstance = ::GetModuleHandle(NULL);
	wcex.lpszClassName = L"ConsoleOverlayWindow-CCBE8432-6AC5-476C-8EE6-E4E21DB90138";
	wcex.lpfnWndProc = SimpleWindow::Static_WndProc;
#ifdef _DEBUG
	wcex.hbrBackground = ::CreateSolidBrush(RGB(255, 0, 0));
	wcex.hCursor = ::LoadCursor(NULL, IDC_CROSS);
#endif

	if (!SimpleWindow::RegisterWindowClass(wcex))
		return false;

	_hWndConsole = ::GetConsoleWindow();
	_consoleWindowThreadId = consoleWindowThreadId;

	DWORD style = WS_POPUP;// WS_OVERLAPPEDWINDOW;
	DWORD exstyle = WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_ACCEPTFILES;

	// making the console window as the owner will cause this window to always stay above it.
	// in addition when the console window is minimized this window will be automatically hidden as well.
	HWND hWnd = CreateWindowEx(exstyle, 
		wcex.lpszClassName, nullptr, style, 
#ifdef _DEBUG
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
#else
		0, 0, 0, 0,
#endif
		//NULL,
		_hWndConsole, 		// use console's window as the owner
		nullptr, wcex.hInstance, this);

	if (!hWnd)
	{
		debug_print("ConsoleOverlayWindow::Create - CreateWindowEx failed, err=%#x\n", ::GetLastError());
		return false;
	}

#ifdef _DEBUG
	::SetLayeredWindowAttributes(hWnd, 0, 100, LWA_ALPHA);
#else
	::SetLayeredWindowAttributes(hWnd, 0, 1, LWA_ALPHA);
#endif

	_hWndOverlay = hWnd;
	debug_print("ConsoleOverlayWindow::Create - overlay window created, hwnd=%#x\n", _hWndOverlay);

	if (!_selectionView.Create(_hWndConsole, _hWndOverlay))
		return false;

	//SetConsoleWindowAsTheOwner();

	ReOpenConsoleHandles();

	SetupWinEventHooks();

//#ifdef _DEBUG
	::ShowWindow(hWnd, SW_SHOWNOACTIVATE);
	::UpdateWindow(hWnd);
//#endif

	AdjustOverlayPosition();

	AdjustOverlayZOrder();

	if (!_sizeTooltip.Create(_hWndOverlay))
		debug_print("ConsoleOverlayWindow::Create - _sizeTooltip.Create failed, err=%#x\n", ::GetLastError());

	_resizeOperation.OnSizeChanged([this]() {
		auto size = _resizeOperation.GetRectangle().size();
		auto changed = ResizeConsole(size);
		AdjustOverlayPosition();

		if (_resizeOperation.IsActive())
		{
			if (changed)
				UpdateSizeTooltipText();
		}
		else
			_sizeTooltip.Show(false);
	});

	::DragAcceptFiles(_hWndOverlay, TRUE);

	return true;
}

void ConsoleOverlayWindow::SetupWinEventHooks()
{
	_winEventThunk.reset(
		[this](HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
	{
		switch (event)
		{
		case EVENT_CONSOLE_CARET:
			if (_selectionHelper.IsShowing())
			{
				_selectionHelper.Clear();
			}
			break;
		case EVENT_CONSOLE_UPDATE_SCROLL:
			//debug_print("CONSOLE_SCROLL - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_CONSOLE_LAYOUT:
			if (_selectionHelper.IsShowing())
			{
				_consoleHelper.RefreshInfo();
				_selectionView.Refresh();
			}
			//debug_print("CONSOLE_LAYOUT - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_MOVESIZESTART:
			break;
		case EVENT_SYSTEM_MOVESIZEEND:
			AdjustOverlayPosition();
			break;
		case EVENT_SYSTEM_MINIMIZESTART:
			break;
		case EVENT_SYSTEM_MINIMIZEEND:
			AdjustOverlayPosition();
			break;
		case EVENT_SYSTEM_FOREGROUND:
			debug_print("SYSTEM_FOREGROUND - %ld %ld\n", idObject, idChild);
			AdjustOverlayZOrder();
			break;
		case EVENT_OBJECT_REORDER:
			// this seems to fire when someone is changing the active console's screen
			// buffer with a call to SetConsoleActiveScreenBuffer.
			// unfortunately it also fires in many other occasions as well, like when
			// console resized, minimized, restored...
			if (!_resizeOperation.IsActive() && (!idObject && !idChild))
			{
				debug_print("EVENT_OBJECT_REORDER - %ld %ld\n", idObject, idChild);
				ReOpenConsoleHandles();
				AdjustOverlayPosition();
			}
			break;
		case EVENT_OBJECT_LOCATIONCHANGE:
			// TODO: dirty static hack. FIXME. 
			static auto isZoomed = ::IsZoomed(_hWndConsole);

			if (hWnd == _hWndConsole)
			{
				if (isZoomed != ::IsZoomed(hWnd))
				{
					isZoomed = ::IsZoomed(hWnd);
					AdjustOverlayPosition();
				}

				if (_selectionHelper.IsShowing())
					_selectionView.AdjustPosition();
			}

			break;
		}
	});

	auto hHooks = {
		::SetWinEventHook(EVENT_CONSOLE_CARET, EVENT_CONSOLE_CARET, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_CONSOLE_UPDATE_SCROLL, EVENT_CONSOLE_LAYOUT, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
		::SetWinEventHook(EVENT_OBJECT_REORDER, EVENT_OBJECT_REORDER, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS),
	};

	for (auto h : hHooks)
		_winEventHooks.push_back(scoped_wineventhook(h));
}

void ConsoleOverlayWindow::SetConsoleWindowAsTheOwner()
{
	_hWndConsole = ::GetConsoleWindow();

	::SetWindowLongPtr(_hWndOverlay, GWL_HWNDPARENT, (LONG_PTR)_hWndConsole);

	HWND hWndOwner = (HWND)::GetWindowLongPtr(_hWndOverlay, GWL_HWNDPARENT);
	if (hWndOwner != _hWndConsole)
		debug_print("ConsoleOverlayWindow::AttachToConsole - failed\n");
}

void ConsoleOverlayWindow::AdjustOverlayZOrder()
{
	if (!::IsWindowVisible(_hWndConsole) || ::IsMinimized(_hWndConsole))
		return;

	// find the first visible toplevel window that is just above the console window
	// and place the overlay window below it.
	// the overlay window will sit just above the console window, but also below all
	// other toplevel windows, including the console's properties dialog window.

	// TODO: what if the console window is topmost? (WS_EX_TOPMOST)

	auto hWnd = _hWndConsole;
	while (hWnd = ::GetWindow(hWnd, GW_HWNDPREV))
	{
		if (::IsWindowVisible(hWnd) && !::IsMinimized(hWnd))
		{
			::SetWindowPos(_hWndOverlay, hWnd,
				0, 0, 0, 0,
				SWP_NOACTIVATE
				| SWP_NOMOVE
				| SWP_NOSIZE
				| SWP_NOREPOSITION
				| SWP_NOSENDCHANGING);

			break;
		}
	}

	_selectionView.AdjustPosition();
}

void ConsoleOverlayWindow::ReOpenConsoleHandles()
{
	_consoleHelper.ReOpenHandles();
	_consoleHelper.RefreshInfo();
}

void ConsoleOverlayWindow::AdjustOverlayPosition()
{
	if (_resizeOperation.IsActive())
		return;

	// the overlay window should have the exact same size as the console window
	// and should be placed directly above it.

	// by covering the borders and the client area, the overlay window can override and supply
	// new functionality, like better console resizing and modern selection support.

	// note that the console's caption area and the scrollbars wont be covered thought.
	// it will be done by cutting out specific regions from the overlay window.

	auto consoleWindowScrollbarsStyle = ::GetWindowLongPtr(_hWndConsole, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL);
	auto overlayWindowScrollbarsStyle = ::GetWindowLongPtr(_hWndOverlay, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL);

	auto consoleWindowRect = GetWindowRect(_hWndConsole);
	auto overlayWindowRect = GetWindowRect(_hWndOverlay);

	// if the console's size or scrollbars were changed then the clipping areas of the overlay window need to be fixed
	if ((overlayWindowRect.size() != consoleWindowRect.size()) ||
		(consoleWindowScrollbarsStyle != overlayWindowScrollbarsStyle))
	{
		debug_print("AdjustOverlayPosition: width or style changed\n");

		// get console's client area rectangle in global desktop coordinates
		rectangle consoleClientRect{
			MapWindowPoints(_hWndConsole, HWND_DESKTOP, GetClientRect(_hWndConsole))
		};

		// TODO: maybe use GetSystemMetrics(..) instead? SM_CXBORDER? i couldn't figure it out thought
		// there are also GetThemeSysSize and GetThemeMetric to consider
		auto borderSize = consoleClientRect.left() - consoleWindowRect.left();
		
		// create the clipping region of the overlay window.
		// start with a region that covers the whole window and then cut off the caption and the scrollbars areas.
		scoped_gdi_region overlayRegion{ 
			::CreateRectRgn(0, 0, consoleWindowRect.width(), consoleWindowRect.height()) 
		};

		// cut off the region that represents the caption area
		CutoffRegion(overlayRegion, rectangle(
			borderSize, 0,
			consoleWindowRect.width() - 2 * borderSize,
			consoleClientRect.top() - consoleWindowRect.top()
		));

		// cut off the vertical scrollbar area if it exists
		if (consoleWindowScrollbarsStyle & WS_VSCROLL)
		{
			CutoffRegion(overlayRegion, rectangle(
				consoleClientRect.right() - consoleWindowRect.left(),
				consoleClientRect.top() - consoleWindowRect.top(),
				::GetSystemMetrics(SM_CXVSCROLL),
				consoleClientRect.height()
			));
		}

		// cut off the horizontal scrollbar area if it exists
		if (consoleWindowScrollbarsStyle & WS_HSCROLL)
		{
			CutoffRegion(overlayRegion, rectangle(
				consoleClientRect.left() - consoleWindowRect.left(),
				consoleClientRect.bottom() - consoleWindowRect.top(),
				consoleClientRect.width(),
				GetSystemMetrics(SM_CYHSCROLL)
			));
		}

		// update the overlay window's clipping region
		if (::SetWindowRgn(_hWndOverlay, overlayRegion.get(), TRUE))
			// if successfull then windows now owns the above region handle
			// so we need to detach our scoped_resource or else it will be (auto)deleted
			overlayRegion.detach();
	}

	//AdjustOverlayZOrder();

	if (overlayWindowRect != consoleWindowRect)
	{
		debug_print("AdjustOverlayPosition: %ld,%ld %ldx%ld\n", 
			consoleWindowRect.left(), consoleWindowRect.top(),
			consoleWindowRect.width(), consoleWindowRect.height());

		::SetWindowPos(_hWndOverlay, nullptr,
			consoleWindowRect.left(), consoleWindowRect.top(),
			consoleWindowRect.width(), consoleWindowRect.height(),
			SWP_NOACTIVATE
			//| SWP_ASYNCWINDOWPOS
			| SWP_NOREPOSITION
			| SWP_NOZORDER
			| SWP_NOSENDCHANGING);

		_selectionView.AdjustPosition();
	}
}

bool ConsoleOverlayWindow::ResizeConsole(size const& requestedWindowSize)
{
	if (!_consoleHelper.RefreshInfo())
		return false;

	auto currentConsoleSize = GetWindowRect(_hWndConsole).size();

	auto addedCells = (requestedWindowSize - currentConsoleSize) / _consoleHelper.CellSize();
	if (!addedCells)
		return false;

	auto currBufferView = _consoleHelper.BufferView();
	auto currBufferSize = _consoleHelper.BufferSize();
	auto caretPosition = _consoleHelper.CaretPos();

	auto newBufferView = currBufferView + addedCells;

	if (newBufferView.width() < 20)
		newBufferView.width() = 20;

	if (newBufferView.height() < 10)
		newBufferView.height() = 10;

	// the new buffer will be atleast the size of the view
	auto newBufferSize = newBufferView.size();

	// if currently the console has a vertical scrollbar..
	// TODO: and what about a horizontal bar?
	bool isVScroll = (currBufferSize.height() != currBufferView.height());
	if (isVScroll)
	{
		// match the new buffer scroll height to be the same as before.
		newBufferSize.height() = currBufferSize.height();

		// if the caret is currently visible, but due to user shrinking the height of the
		// console for example the caret wont be seen in the new buffer's view, then in
		// that case adjust the view so that the caret will be seen at the last row.
		if (currBufferView.contains(caretPosition) && !newBufferView.contains(caretPosition))
			newBufferView.top() = caretPosition.y() - newBufferView.height() + 1;

		// align the buffer's view to the top of the buffer is needed.
		if (newBufferView.top() < 0)
			newBufferView.top() = 0;

		// align the buffer's view to the bottom of the buffer is needed.
		if (newBufferView.bottom() > newBufferSize.height())
			newBufferView.top() = newBufferSize.height() - newBufferView.height();
	}

	// update the console with the new size but only if something actually changed
	if ((currBufferSize != newBufferSize) || (currBufferView != newBufferView))
	{
		rectangle minRect{
			newBufferView.left(),
			newBufferView.top(),
			std::min(currBufferView.width(), newBufferView.width()),
			std::min(currBufferView.height(), newBufferView.height()),
		};

		::SendMessage(_hWndConsole, WM_SETREDRAW, FALSE, 0);

		if (minRect != currBufferView)
			if (!_consoleHelper.BufferView(minRect))
				debug_print("SetConsoleWindowInfo failed, err=%#x\n", ::GetLastError());

		if (!_consoleHelper.BufferSize(newBufferSize))
			debug_print("SetConsoleScreencurrBufferSize failed, %dx%d err=%#x\n", newBufferSize.width(), newBufferSize.height(), ::GetLastError());

		if (!_consoleHelper.BufferView(newBufferView))
			debug_print("SetConsoleWindowInfo2 failed, err=%#x\n", ::GetLastError());

		::SendMessage(_hWndConsole, WM_SETREDRAW, TRUE, 0);
		::RedrawWindow(_hWndConsole, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

		_selectionView.AdjustPosition();

		return true;
	}

	return false;
}

LRESULT ConsoleOverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (_resizeOperation.HandleMessage(hwnd, msg, wParam, lParam))
		return 0;

	if (_selectionOperation.HandleMessage(hwnd, msg, wParam, lParam))
		return 0;

	if ((msg >= WM_MOUSEFIRST) && (msg <= WM_MOUSELAST))
	{
		if (IsConsoleWantsMouseEvents())
		{
			ForwardConsoleMouseEvent(hwnd, msg, wParam, lParam);
			return 0;
		}
	}

	switch (msg)
	{
	HANDLE_MSG(hwnd, WM_NCHITTEST, OnWM_NCHitTest);
	HANDLE_MSG(hwnd, WM_SETCURSOR, OnWM_SetCursor);

	HANDLE_MSG(hwnd, WM_NCLBUTTONDOWN, OnWM_NCLButtonDown);
	HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnWM_LButtonDown);

	case WM_DROPFILES:
		::PostMessage(_hWndConsole, msg, wParam, lParam);
		break;

	case WM_MOUSEACTIVATE:
		PostMessage(hwnd, WM_USER + 1, 0, 0);
		return MA_NOACTIVATE;

	case WM_NCACTIVATE:
		if (wParam)
			PostMessage(hwnd, WM_USER + 1, 0, 0);
		return TRUE;

	case WM_USER + 1:
		::SetForegroundWindow(_hWndConsole);
		break;

	default:
		return ::DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

UINT ConsoleOverlayWindow::OnWM_NCHitTest(HWND hWnd, int x, int y)
{
	return FORWARD_WM_NCHITTEST(_hWndConsole, x, y, ::DefWindowProc);
}

BOOL ConsoleOverlayWindow::OnWM_SetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
{
	if (codeHitTest != HTCLIENT)
		return FORWARD_WM_SETCURSOR(hwnd, hwndCursor, codeHitTest, msg, ::DefWindowProc);

	if (IsConsoleWantsMouseEvents())
		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
	else
		::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));

	return TRUE;
}

void ConsoleOverlayWindow::OnWM_NCLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT hitTest)
{
	switch (hitTest)
	{
	case HTBOTTOM:
	case HTRIGHT:
	case HTBOTTOMRIGHT:
		_resizeOperation.Start(_hWndOverlay, point(x, y), hitTest);

		_sizeTooltip.SetPosition(MapWindowPoints(_hWndConsole, HWND_DESKTOP, point(10, 10)));
		UpdateSizeTooltipText();
		_sizeTooltip.Show(true);
		break;
	}
}

void ConsoleOverlayWindow::OnWM_LButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	enum SelectionHelper::Mode mode{};

	auto p = MapWindowPoints(hWnd, _hWndConsole, point(x, y));
	auto clicks = DetectMultpleClicks(p.x(), p.y());
	switch (clicks)
	{
	case 1:
		mode = SelectionHelper::SELECT_CHAR;
		break;
	case 2:
		mode = SelectionHelper::SELECT_WORD;
		break;
	case 3:
		mode = SelectionHelper::SELECT_LINE;
		break;
	default:
		_selectionHelper.Clear();
		_selectionOperation.Finish();
		return;
	}

	_selectionOperation.Start(_hWndOverlay, p, mode);
}

bool ConsoleOverlayWindow::IsConsoleWantsMouseEvents() const
{
	if (_resizeOperation.IsActive() || _selectionOperation.IsActive())
		return false;
	
	// if ENABLE_MOUSE_INPUT is set, the default behavior is to forward all mouse events 
	// to the console, but by holding ctrl-alt the user can flip the default behavior.
	// so for example, if ENABLE_MOUSE_INPUT is set (like with Far Manager), by default all 
	// mouse events will be forwarded to the console so that Far Manager will work as expected.
	// as a result though the enhanced selection wont be avaiable, but by holding the modifier 
	// keys all mouse events will be handled so the selection will work.
	// NOTE: cygwin apps running in windows console set the ENABLE_MOUSE_INPUT flag, so to use 
	// the enhanced selection the user needs to hold the modifier keys. (or better yet, run 
	// cygwin apps in mitty and get a better terminal experience all around).

	auto mode = _consoleHelper.InputMode();
	bool isMouseInput = ((mode & ENABLE_MOUSE_INPUT) != 0);

	bool isOverrideKeyPressed = ((::GetAsyncKeyState(VK_MENU) < 0) && (::GetAsyncKeyState(VK_CONTROL) < 0));

	return (isMouseInput && !isOverrideKeyPressed) || (!isMouseInput && isOverrideKeyPressed);
}

void ConsoleOverlayWindow::ForwardConsoleMouseEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto p = MapWindowPoints(hwnd, _hWndConsole, point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));

	switch (msg)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
		{
			auto clicks = DetectMultpleClicks(p.x(), p.y());
			if (clicks == 2)
			{
				switch (msg)
				{
				case WM_LBUTTONDOWN:
					msg = WM_LBUTTONDBLCLK;
					break;
				case WM_RBUTTONDOWN:
					msg = WM_RBUTTONDBLCLK;
					break;
				case WM_MBUTTONDOWN:
					msg = WM_MBUTTONDBLCLK;
					break;
				}
			}
		}
		break;
	}

	::PostMessage(_hWndConsole, msg, wParam, MAKELPARAM(p.x(), p.y()));
}

int ConsoleOverlayWindow::DetectMultpleClicks(int x, int y)
{
	// single/double/triple click detection
	// http://blogs.msdn.com/b/oldnewthing/archive/2004/10/18/243925.aspx

	POINT pt = { x, y };
	DWORD tmClick = ::GetMessageTime();

	if (!::PtInRect(&_clickRect, pt) ||
		tmClick - _lastClickTime > ::GetDoubleClickTime())
	{
		_clicks = 0;
	}

	++_clicks;
	_lastClickTime = tmClick;

	::SetRect(&_clickRect, x, y, x, y);
	::InflateRect(&_clickRect,
		GetSystemMetrics(SM_CXDOUBLECLK) / 2,
		GetSystemMetrics(SM_CYDOUBLECLK) / 2);

	_clicks = _clicks % 4;

	return _clicks;
}

void ConsoleOverlayWindow::UpdateSizeTooltipText()
{
	auto size = _consoleHelper.BufferView().size();

	WCHAR str[20];
	wsprintf(str, L"%d, %d", size.width(), size.height());

	_sizeTooltip.SetText(str);
}
