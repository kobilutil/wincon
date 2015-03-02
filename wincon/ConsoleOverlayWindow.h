#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include "SelectionViewWindow.h"
#include "SelectionHelper.h"
#include "ResizeOperation.h"
#include <vector>


class ConsoleOverlayWindow
{
	using SimpleWindow = SimpleWndProc < ConsoleOverlayWindow > ;
	friend SimpleWindow;

public:
	ConsoleOverlayWindow();
	~ConsoleOverlayWindow();

	bool Create(DWORD consoleWindowThreadId);

private:
	void AdjustOverlayPosition();
	void AdjustOverlayZOrder();
	void SetConsoleWindowAsTheOwner();

	void ReOpenConsoleHandles();

	//void AdjustOverlayPosition();
	void ResizeConsole(size& requestedWindowSize);

	void SetupWinEventHooks();

	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	UINT OnWM_NCHitTest(HWND hwnd, int x, int y);
	BOOL OnWM_SetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg);

	void OnWM_MouseMove(HWND hwnd, int x, int y, UINT keyFlags);
	void OnWM_MouseLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
	void OnWM_MouseLButtonUp(HWND hwnd, int x, int y, UINT keyFlags);

	void OnWM_NCLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest);

	void OnWM_Timer(HWND hwnd, UINT id);

	bool	IsConsoleWantsMouseEvents() const;
	void	ForwardConsoleMouseEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	int		DetectMultpleClicks(int x, int y);
	//point	MapConsolePoint(point const& p);

	void	ScrollConsole(int dy);

private:
	static const auto AUTO_SCROLL_TIMER_ID = 1;
	static const auto AUTO_SCROLL_TIMER_TIMEOUT = 100;

	DWORD	_consoleWindowThreadId;

	HWND	_hWndOverlay;
	HWND	_hWndConsole;

	ConsoleHelper	_consoleHelper;

	int		_clicks;
	RECT	_clickRect;
	DWORD	_lastClickTime;

	StaticThunk<WINEVENTPROC, ConsoleOverlayWindow>	_winEventThunk;
	std::vector<scoped_wineventhook>				_winEventHooks;

	SelectionViewWindow _selectionView;
	SelectionHelper		_selectionHelper;
	point				_selectionLastMousePixelPos;
	bool				_selectionAutoScrollTimer = false;
	int					_selectionAutoScrollTimerDeltaY = 0;

	ResizeOperation		_resizeOperation;
};
