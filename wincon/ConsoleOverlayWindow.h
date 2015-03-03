#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include "SelectionViewWindow.h"
#include "SelectionHelper.h"
#include "ResizeOperation.h"
#include "SelectionOperation.h"
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

	void OnWM_LButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
	void OnWM_NCLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest);

	bool	IsConsoleWantsMouseEvents() const;
	void	ForwardConsoleMouseEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	int		DetectMultpleClicks(int x, int y);
	//point	MapConsolePoint(point const& p);

private:
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

	ResizeOperation		_resizeOperation;
	SelectionOperation	_selectionOperation;
};
