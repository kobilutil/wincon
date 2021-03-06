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
	_hWndConsole = ::GetConsoleWindow();
	_consoleWindowThreadId = consoleWindowThreadId;

	// create the overlay window
	_hWndOverlay = CreateOverlayWindow();
	if (!_hWndOverlay)
		return false;

	// create the selection view window
	if (!_selectionView.Create(_hWndConsole, _hWndOverlay))
		return false;

	// create the tooltip for displaying the console's size while resizing
	if (!_sizeTooltip.Create(_hWndOverlay))
		debug_print("ConsoleOverlayWindow::Create - _sizeTooltip.Create failed, err=%#x\n", ::GetLastError());

	ReOpenConsoleHandles();

	SetupWinEventHooks();

	// place the overlay window directly above the console window
	AdjustOverlayPosition();
	AdjustOverlayZOrder();

	// register for OnSizeChanged notification 
	_resizeOperation.OnSizeChanged([this]() 
	{
		auto changed = ResizeConsole(_resizeOperation.GetRectangle());

		// while the resize operation continues
		if (_resizeOperation.IsActive())
		{
			// update the tooltip to display the current size of the console
			if (changed)
				UpdateSizeTooltipText();
		}
		// when the resize operation finished
		else
		{
			// hide the tooltip and adjust the overlay position for the final size of the console
			_sizeTooltip.Show(false);
			AdjustOverlayPosition();
		}
	});

	// HACK: without this the console's WinEvents are arriving with a delay. it is especially 
	// noticeable when scrolling the console while there is a selection showing.
	// experiments have shown that events are arriving just fine for other applications that
	// are not actually attached to the console.
	// TODO: find out why.
	::SetTimer(NULL, 0, 10, [](HWND, UINT, UINT_PTR, DWORD){
		// an empty timer handler also does the trick.
		// it seems that just by setting a timer, the WinEvents arriving without a delay.
		//::PostMessage(::GetConsoleWindow(), WM_NULL, 0, 0);
		//::PostThreadMessage(::GetCurrentThreadId(), WM_NULL, 0, 0);
	});

	return true;
}

HWND ConsoleOverlayWindow::CreateOverlayWindow()
{
	::WNDCLASSEX wcex{};
	wcex.cbSize = sizeof(wcex);
	wcex.hInstance = ::GetModuleHandle(NULL);
	wcex.lpszClassName = L"ConsoleOverlayWindow-CCBE8432-6AC5-476C-8EE6-E4E21DB90138";
	wcex.lpfnWndProc = SimpleWindow::Static_WndProc;
	wcex.hbrBackground = GetStockBrush(HOLLOW_BRUSH);
#ifdef _DEBUG
	wcex.hbrBackground = ::CreateSolidBrush(RGB(255, 0, 0));
	wcex.hCursor = ::LoadCursor(NULL, IDC_CROSS);
#endif

	// NOTE: without the following set there are drawing artifacts especialy when maximizing/restoring 
	// the console. it seems like part of the window becomes "dirty" and loose its transparency property.
	// TODO: find out why.
	wcex.style = CS_HREDRAW | CS_VREDRAW;

	if (!SimpleWindow::RegisterWindowClass(wcex))
		return false;

	// making the console window as the owner will cause this window to always stay above it.
	// in addition when the console window is minimized this window will be automatically hidden as well.
	auto hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_ACCEPTFILES,
		wcex.lpszClassName, NULL, WS_POPUP, 0, 0, 0, 0, _hWndConsole, NULL, wcex.hInstance, this);
	if (!hWnd)
	{
		debug_print("ConsoleOverlayWindow::CreateOverlayWindow - CreateWindowEx failed, err=%#x\n", ::GetLastError());
		return NULL;
	}

	debug_print("ConsoleOverlayWindow::CreateOverlayWindow - overlay window created, hwnd=%#x\n", hWnd);

	// setup window for alpha transperancy
#ifdef _DEBUG
	::SetLayeredWindowAttributes(hWnd, 0, 100, LWA_ALPHA);
#else
	::SetLayeredWindowAttributes(hWnd, 0, 1, LWA_ALPHA);
#endif

	// mark that the overlay window can accept dropping files onto it
	::DragAcceptFiles(hWnd, TRUE);

	return hWnd;
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
				_selectionHelper.Clear();
			break;
		case EVENT_CONSOLE_UPDATE_SCROLL:
			//debug_print("CONSOLE_SCROLL - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_CONSOLE_LAYOUT:
			//debug_print("CONSOLE_LAYOUT - %ld %ld\n", idObject, idChild);

			DetectConsoleMaximizedRestoredStates();

			if (_selectionHelper.IsShowing())
			{
				_consoleHelper.RefreshInfo();
				_selectionView.Refresh();
			}
			break;
		case EVENT_SYSTEM_MOVESIZESTART:
			StartResizeOperation();
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
			// this event fires on many occasions and especially alot during moving or resizing,
			// however we only interested in identifying when the console is maximized and restored.
			if ((hWnd == _hWndConsole) && (idObject == OBJID_WINDOW))
			{
				//debug_print("LOCATIONCHANGE - %ld %ld\n", idObject, idChild);

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

	// the overlay window should have the exact same size as the console's client area
	// and should be placed directly above it.

	auto consoleClientRect = MapWindowPoints(_hWndConsole, HWND_DESKTOP, GetClientRect(_hWndConsole));
	auto overlayWindowRect = GetWindowRect(_hWndOverlay);

	//AdjustOverlayZOrder();

	if (overlayWindowRect != consoleClientRect)
	{
		debug_print("AdjustOverlayPosition: %ld,%ld %ldx%ld\n", 
			consoleClientRect.left(), consoleClientRect.top(),
			consoleClientRect.width(), consoleClientRect.height());

		::SetWindowPos(_hWndOverlay, nullptr,
			consoleClientRect.left(), consoleClientRect.top(),
			consoleClientRect.width(), consoleClientRect.height(),
			SWP_NOACTIVATE
			| SWP_NOREPOSITION
			| SWP_NOZORDER
			| SWP_NOSENDCHANGING
			| SWP_SHOWWINDOW);

		_selectionView.AdjustPosition();
	}
}

bool ConsoleOverlayWindow::ResizeConsole(rectangle const& requestedSizePos)
{
	if (!_consoleHelper.RefreshInfo())
		return false;

	auto currentPixelSize = GetWindowRect(_hWndConsole).size();

	auto addedCells = (requestedSizePos.size() - currentPixelSize) / _consoleHelper.CellSize();
	if (!addedCells)
		return false;

	auto newBufferView = _consoleHelper.BufferView() + addedCells;
	auto rc = _consoleHelper.Resize(newBufferView);

	if (rc)
	{
		auto newPixelSize = GetWindowRect(_hWndConsole);
		if (newPixelSize.top_left() != requestedSizePos.top_left())
		{
			::SetWindowPos(_hWndConsole, NULL, requestedSizePos.left(), requestedSizePos.top(),
				0, 0, SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER);
		}
	}

	return rc;
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
	HANDLE_MSG(hwnd, WM_SETCURSOR, OnWM_SetCursor);

	HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnWM_LButtonDown);

	case WM_RBUTTONDOWN:
		PasteFromClipboardToConsole();
		break;

	case WM_MBUTTONDOWN:
		PasteFromClipboardToConsole();
		break;

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

BOOL ConsoleOverlayWindow::OnWM_SetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
{
	// note that the codeHitTest is actually the one we got from the console
	if (codeHitTest != HTCLIENT)
		return ::SendMessage(hwnd, WM_SETCURSOR, (WPARAM)_hWndConsole, MAKELPARAM(codeHitTest, msg));

	if (IsConsoleWantsMouseEvents())
		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
	else
		::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));

	return TRUE;
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

void ConsoleOverlayWindow::StartResizeOperation()
{
	auto p = GetCursorPos();
	auto hitTest = ::SendMessage(_hWndConsole, WM_NCHITTEST, 0, MAKELPARAM(p.x(), p.y()));

	switch (hitTest)
	{
	case HTLEFT:
	case HTRIGHT:
	case HTTOP:
	case HTTOPLEFT:
	case HTTOPRIGHT:
	case HTBOTTOM:
	case HTBOTTOMLEFT:
	case HTBOTTOMRIGHT:

		// cancel the current operation
		ReleaseCapture();
		SetCapture(_hWndOverlay);
		ReleaseCapture();

		_resizeOperation.Start(_hWndOverlay, p, hitTest);

		UpdateSizeTooltipText();
		_sizeTooltip.Show(true);
		break;
	}
}

void ConsoleOverlayWindow::DetectConsoleMaximizedRestoredStates()
{
	auto isZoomed = ::IsZoomed(_hWndConsole);

	// if the console has just been maximized
	if (isZoomed && !_zoomState.isZoomed)
	{
		// save the previous size
		_zoomState.normalSize = _consoleHelper.BufferView().size();

		// resize to console's buffer view to maximum available
		auto maxSize = _consoleHelper.LargestViewSize();

		_consoleHelper.RefreshInfo();
		_consoleHelper.Resize(maxSize);
	}
	// if the console has just been restored
	else if (!isZoomed && _zoomState.isZoomed)
	{
		// restore the previous size
		if (_zoomState.normalSize)
		{
			_consoleHelper.RefreshInfo();
			_consoleHelper.Resize(_zoomState.normalSize);
		}
	}

	// if the console changed from normal to maximized or vice versa
	// then make sure the overlay is positioned correctly.
	if (_zoomState.isZoomed != isZoomed)
	{
		_zoomState.isZoomed = isZoomed;
		AdjustOverlayPosition();
	}
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

	// for now use only the ctrl key as an override key since holding the alt key seems to 
	// prevent the console from opening its native context menu.
	// TODO: consider instead of holding the modifier key(s) the user can alternate between 2 states
	//bool isOverrideKeyPressed = ((::GetAsyncKeyState(VK_MENU) < 0) && (::GetAsyncKeyState(VK_CONTROL) < 0));
	bool isOverrideKeyPressed = (::GetAsyncKeyState(VK_CONTROL) < 0);

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

	_sizeTooltip.SetPosition(MapWindowPoints(_hWndConsole, HWND_DESKTOP, point(10, 10)));
}

void ConsoleOverlayWindow::PasteFromClipboardToConsole()
{
	std::wstring buffer;

	// TODO: use scoped_resource
	if (::OpenClipboard(_hWndOverlay))
	{
		auto hMem = ::GetClipboardData(CF_UNICODETEXT);
		if (hMem)
		{
			auto pMem = ::GlobalLock(hMem);
			if (pMem)
			{
				buffer.assign((LPCWSTR)pMem);
				::GlobalUnlock(hMem);
			}
		}
		::CloseClipboard();
	}

	_consoleHelper.WriteInput(buffer.c_str());
}