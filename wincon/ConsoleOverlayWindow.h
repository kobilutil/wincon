#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include "utils\TrackingTooltip.h"
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
	HWND CreateOverlayWindow();

	void AdjustOverlayPosition();
	void AdjustOverlayZOrder();
	void SetConsoleWindowAsTheOwner();

	void ReOpenConsoleHandles();

	bool ResizeConsole(size const& requestedWindowSize);

	void SetupWinEventHooks();

	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL OnWM_SetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg);
	void OnWM_LButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);

	void StartResizeOperation();

	void DetectConsoleMaximizedRestoredStates();

	bool IsConsoleWantsMouseEvents() const;
	void ForwardConsoleMouseEvent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	int DetectMultpleClicks(int x, int y);
	//point	MapConsolePoint(point const& p);

	void UpdateSizeTooltipText();

	void PasteFromClipboardToConsole();

private:
	DWORD	_consoleWindowThreadId;

	HWND	_hWndOverlay;
	HWND	_hWndConsole;

	ConsoleHelper	_consoleHelper;

	int		_clicks;
	RECT	_clickRect;
	DWORD	_lastClickTime;

	struct {
		BOOL	isZoomed = FALSE;
		size	normalSize;
	}
	_zoomState;

	StaticThunk<WINEVENTPROC, ConsoleOverlayWindow>	_winEventThunk;
	std::vector<scoped_wineventhook>				_winEventHooks;

	SelectionViewWindow _selectionView;
	SelectionHelper		_selectionHelper;

	ResizeOperation		_resizeOperation;
	SelectionOperation	_selectionOperation;

	TrackingTooltip		_sizeTooltip;
};
