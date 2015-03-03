#pragma once
#include "utils\utils.h"
#include "SelectionHelper.h"

class SelectionOperation
{
	static const auto AUTO_SCROLL_TIMER_ID = 1;
	static const auto AUTO_SCROLL_TIMER_TIMEOUT = 100;

public:
	SelectionOperation(ConsoleHelper& _consoleHelper, SelectionHelper& _selectionHelper);

	void Start(HWND hWnd, point const& anchor, enum SelectionHelper::Mode mode);
	void Finish();

	bool IsActive() const	{ return _isActive; }

	bool HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	void EnableAutoScrollTimer(int dy);
	void DisableAutoScrollTimer();

	void OnWM_MouseMove(HWND hWnd, int x, int y, UINT keyFlags);

	void ScrollConsole(int dy);

private:
	bool	_isActive = false;
	HWND	_hWnd;

	point	_lastMousePixelPos;
	bool	_autoScrollTimer = false;
	int		_autoScrollTimerDeltaY = 0;

	ConsoleHelper&		_consoleHelper;
	SelectionHelper&	_selectionHelper;
};

