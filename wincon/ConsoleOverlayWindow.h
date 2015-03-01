#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include "SelectionViewWindow.h"
#include "SelectionHelper.h"
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
	void OnWM_MouseButtonDown(HWND hwnd, int x, int y, UINT keyFlags);
	void OnWM_MouseButtonUp(HWND hwnd, int x, int y, UINT keyFlags);

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
	point				_selectionLastMousePixelPos;

	class ResizeHelper
	{
	public:
		ResizeHelper() : _isResizing(false)
		{}

		void Start(UINT direction, point const& anchor, rectangle const& initialRect)
		{
			_isResizing = true;
			_direction = direction;
			_anchor = anchor;
			_initialRect = initialRect;
			_currentRect = initialRect;
		}

		void Finish()
		{
			_isResizing = false;
		}

		void ResizeTo(point const& p)
		{
			_currentRect = _initialRect;

			if (_direction == HTRIGHT || _direction == HTBOTTOMRIGHT)
				_currentRect.size().width() += (p.x() - _anchor.x());

			if (_direction == HTBOTTOM || _direction == HTBOTTOMRIGHT)
				_currentRect.size().height() += (p.y() - _anchor.y());
		
			//_anchor = p;
		}

		bool IsResizing() const			{ return _isResizing; }
		rectangle GetRectangle() const	{ return _currentRect; }

	private:
		bool		_isResizing;
		UINT		_direction;
		point		_anchor;
		rectangle	_initialRect;
		rectangle	_currentRect;
	} 
	_resizeHelper;

};
