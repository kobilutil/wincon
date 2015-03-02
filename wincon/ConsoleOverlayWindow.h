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

	class ResizeOperation
	{
	public:
		void Start(HWND hWnd, point const& anchor, UINT direction)
		{
			if (IsActive())
				Finish();

			debug_print("ResizeOperation::Start - anchor=(%d,%d), dir=%d\n", 
				anchor.x(), anchor.y(), direction);

			_hWnd = hWnd;
			_anchor = anchor;
			_direction = direction;
			_initialRect = GetWindowRect(_hWnd);
			_currentRect = _initialRect;
			_isActive = true;

			::SetCapture(_hWnd);
		}

		void Finish()
		{
			if (!IsActive())
				return;

			debug_print("ResizeOperation::Finish\n");
				
			_hWnd = NULL;
			_isActive = false;
			::ReleaseCapture();

			FireSizeChangedEvent();
		}

		bool IsActive() const					{ return _isActive; }
		rectangle const& GetRectangle() const	{ return _currentRect; }

		bool HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			if (!IsActive() || (hWnd != _hWnd))
				return false;

			switch (msg)
			{
			case WM_MOUSEMOVE:
				//ResizeTo(MapWindowPoints(_hWnd, HWND_DESKTOP, point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))));
				ResizeTo(GetCursorPos());
				break;
			case WM_LBUTTONUP:
			case WM_CAPTURECHANGED:
				Finish();
				break;
			default:
				return false;
			}
			return true;
		}

		template <typename T>
		void OnSizeChanged(T func) { _sizeChangedEvent.push_back(func); }

	private:
		void FireSizeChangedEvent()
		{
			for (auto& func : _sizeChangedEvent)
				func();
		}

		void ResizeTo(point const& p)
		{
			_currentRect = _initialRect;

			if (_direction == HTRIGHT || _direction == HTBOTTOMRIGHT)
				_currentRect.size().width() += (p.x() - _anchor.x());

			if (_direction == HTBOTTOM || _direction == HTBOTTOMRIGHT)
				_currentRect.size().height() += (p.y() - _anchor.y());

			FireSizeChangedEvent();
		}

	private:
		bool		_isActive = false;
		HWND		_hWnd;
		point		_anchor;
		UINT		_direction;
		rectangle	_initialRect;
		rectangle	_currentRect;

		std::vector< std::function<void(void)> > _sizeChangedEvent;
	}
	_resizeOperation;
};
