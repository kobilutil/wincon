#include "stdafx.h"
#include "SelectionViewWindow.h"
#include <windowsx.h>


SelectionViewWindow::SelectionViewWindow(ConsoleHelper& consoleHelper, SelectionHelper& selectionHelper) :
	_consoleHelper(consoleHelper),
	_selectionHelper(selectionHelper)
{}

SelectionViewWindow::~SelectionViewWindow()
{}


bool SelectionViewWindow::Create(HWND hWndConsole, HWND hWndOverlay)
{
	::WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(wcex);
	wcex.hInstance = ::GetModuleHandle(NULL);
	wcex.lpszClassName = L"SelectionViewWindow-CCBE8432-6AC5-476C-8EE6-E4E21DB90138";
	wcex.lpfnWndProc = SimpleWndProc::Static_WndProc;
	wcex.hbrBackground = GetStockBrush(BLACK_BRUSH);
	//wcex.hbrBackground = ::GetSysColorBrush(COLOR_HIGHLIGHT);

	if (!SimpleWndProc::RegisterWindowClass(wcex))
		return false;

	_hWndConsole = hWndConsole;
	_hWndOverlay = hWndOverlay;

	DWORD style = WS_POPUP;// WS_OVERLAPPEDWINDOW;
	DWORD exstyle = WS_EX_LAYERED | WS_EX_NOACTIVATE;

	HWND hWnd = ::CreateWindowEx(exstyle,
		wcex.lpszClassName, NULL, style,
		0, 0, 0, 0,
		//NULL,
		_hWndConsole, 		// use console's window as the owner
		NULL, wcex.hInstance, this);

	if (!hWnd)
	{
		debug_print("SelectionViewWindow::Create - CreateWindowEx failed, err=%#x\n", ::GetLastError());
		return false;
	}

	::SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 100, LWA_ALPHA | LWA_COLORKEY);

	::ShowWindow(hWnd, SW_SHOWNOACTIVATE);
	::UpdateWindow(hWnd);

	_hWnd = hWnd;
	debug_print("SelectionViewWindow::Create - selection view window created, hwnd=%#x\n", _hWnd);

	// register for selection-changed event
	_selectionHelper.OnSelectionChanged([this](){ Refresh(); });

	return true;
}

void SelectionViewWindow::AdjustPosition()
{
	auto rect = MapWindowPoints(_hWndConsole, HWND_DESKTOP, GetClientRect(_hWndConsole));

	::SetWindowPos(_hWnd, _hWndOverlay,
		rect.left(), rect.top(), rect.width(), rect.height(),
		SWP_NOACTIVATE | SWP_NOSENDCHANGING);

	//debug_print("SelectionViewWindow::AdjustPosition - %d,%d %dx%d\n",
	//	rect.left(), rect.top(), rect.width(), rect.height());
}

void SelectionViewWindow::Refresh()
{
	if (_selectionHelper.IsShowing())
	{
		auto cellSize = _consoleHelper.CellSize();

		auto p1 = _consoleHelper.MapCellToPixel(_selectionHelper.p1());
		auto p2 = _consoleHelper.MapCellToPixel(_selectionHelper.p2());

		auto clientArea = GetClientRect(_hWnd);
		scoped_gdi_region region{
			::CreateRectRgn(0, p1.y(), clientArea.right(), p2.y() + cellSize.height())
		};

		CutoffRegion(region, rectangle(0, p1.y(), p1.x(), cellSize.height()));
		CutoffRegion(region, rectangle(p2.x() + cellSize.width(), p2.y(), clientArea.right() - (p2.x() + cellSize.width()), cellSize.height()));

		_selectionRegion = std::move(region);
	}
	else
	{
		_selectionRegion.reset();
	}

	::InvalidateRect(_hWnd, nullptr, TRUE);
}

LRESULT SelectionViewWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		HANDLE_MSG(hwnd, WM_ERASEBKGND, OnWM_EraseBackground);
		HANDLE_MSG(hwnd, WM_PAINT, OnWM_Paint);

	default:
		return ::DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL SelectionViewWindow::OnWM_EraseBackground(HWND hwnd, HDC hdc)
{
	return TRUE;
}

void SelectionViewWindow::OnWM_Paint(HWND hWnd)
{
	PAINTSTRUCT ps;
	::BeginPaint(hWnd, &ps);

	auto clientArea = GetClientRect(_hWnd);

	scoped_gdi_region bgregion{
		::CreateRectRgn(clientArea.left(), clientArea.top(), clientArea.right(), clientArea.bottom())
	};

	if (_selectionRegion)
	{
		::FillRgn(ps.hdc, _selectionRegion.get(), ::GetSysColorBrush(COLOR_HIGHLIGHT));

		::FrameRgn(ps.hdc, _selectionRegion.get(), GetStockBrush(WHITE_BRUSH), 1, 1);

		// cut off the selection region from the background region
		::CombineRgn(bgregion.get(), bgregion.get(), _selectionRegion.get(), RGN_DIFF);
	}

	::FillRgn(ps.hdc, bgregion.get(), GetStockBrush(BLACK_BRUSH));

	::EndPaint(hWnd, &ps);
}
