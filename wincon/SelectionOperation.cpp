#include "stdafx.h"
#include "SelectionOperation.h"
#include <WindowsX.h>

SelectionOperation::SelectionOperation(ConsoleHelper& consoleHelper, SelectionHelper& selectionHelper) :
	_consoleHelper(consoleHelper),
	_selectionHelper(selectionHelper)
{}

void SelectionOperation::Start(HWND hWnd, point const& anchor, enum SelectionHelper::Mode mode)
{
	if (IsActive())
		Finish();

	debug_print("SelectionOperation::Start - anchor=(%d,%d), mode=%d\n",
		anchor.x(), anchor.y(), 0);

	_hWnd = hWnd;
	_isActive = true;

	_consoleHelper.RefreshInfo();
	_selectionHelper.Start(_consoleHelper.MapPixelToCell(anchor), mode);

	::SetCapture(_hWnd);
}

void SelectionOperation::Finish()
{
	if (!IsActive())
		return;

	debug_print("SelectionOperation::Finish\n");

	_selectionHelper.Finish();
	_selectionHelper.CopyToClipboard(_hWnd);

	DisableAutoScrollTimer();

	_hWnd = NULL;
	_isActive = false;

	::ReleaseCapture();
}

bool SelectionOperation::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!IsActive() || (hWnd != _hWnd))
		return false;

	switch (msg)
	{
	HANDLE_MSG(hWnd, WM_MOUSEMOVE, OnWM_MouseMove);

	case WM_TIMER:
		if (wParam != AUTO_SCROLL_TIMER_ID)
			return false;
		ScrollConsole(_autoScrollTimerDeltaY);
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

void SelectionOperation::EnableAutoScrollTimer(int dy)
{
	if (!_autoScrollTimer)
	{
		::SetTimer(_hWnd, AUTO_SCROLL_TIMER_ID, AUTO_SCROLL_TIMER_TIMEOUT, NULL);
		_autoScrollTimer = true;
	}
	_autoScrollTimerDeltaY = dy;
}

void SelectionOperation::DisableAutoScrollTimer()
{
	if (_autoScrollTimer)
	{
		::KillTimer(_hWnd, AUTO_SCROLL_TIMER_ID);
		_autoScrollTimer = false;
	}
	_autoScrollTimerDeltaY = 0;
}

void SelectionOperation::OnWM_MouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
	auto hWndConsole = ::GetConsoleWindow();

	_consoleHelper.RefreshInfo();

	//auto mousePixelPos = MapWindowPoints(hWnd, _hWndConsole, point(x, y));
	auto mousePixelPos = MapWindowPoints(HWND_DESKTOP, hWndConsole, GetCursorPos());
	auto mouseCellPos = _consoleHelper.MapPixelToCell(mousePixelPos);

	auto bufferView = _consoleHelper.BufferView();

	// adjust selection position so it will always fall inside the visible part of the console's buffer.
	if (mouseCellPos.y() < bufferView.top())
		mouseCellPos.y() = bufferView.top();
	else if (mouseCellPos.y() >= bufferView.bottom())
		mouseCellPos.y() = bufferView.bottom() - 1;

	// actually extend the selection
	_selectionHelper.ExtendTo(mouseCellPos);

	// revaluate the autoscrolling only if the user actually moves the mouse. note that if the auto
	// scroll timer is currently active the console will continue to slowly scroll, but if the user
	// start moving the mouse as well then the scrolling will be much much faster. this is somewhat
	// similar to how notepad++ behaves.
	if (mousePixelPos == _lastMousePixelPos)
		return;

	_lastMousePixelPos = mousePixelPos;

	auto scrollDeltaY = 0;
	auto rect = GetClientRect(hWndConsole);

	// if the mouse is above the console window, then scroll up if possible
	if (mousePixelPos.y() < rect.top())
		scrollDeltaY = -1;
	// if the mouse is below the console window, then scroll down if possible
	else if (mousePixelPos.y() >= rect.bottom())
		scrollDeltaY = +1;

	// if need to scroll..
	if (scrollDeltaY)
	{
		// actually do the scrolling
		ScrollConsole(scrollDeltaY);

		// setup the auto scroll timer if needed
		EnableAutoScrollTimer(scrollDeltaY);
	}
	// if no need to scroll..
	else
	{
		// stop the auto scroll timer if it is active
		DisableAutoScrollTimer();
	}
}

void SelectionOperation::ScrollConsole(int dy)
{
	if (!dy)
		return;

	auto bufferView = _consoleHelper.BufferView();
	bufferView.top() += dy;

	if (bufferView.top() < 0)
		bufferView.top() = 0;

	auto bufferSize = _consoleHelper.BufferSize();
	if (bufferView.bottom() >= bufferSize.height())
		bufferView.top() = bufferSize.height() - bufferView.height();

	_consoleHelper.BufferView(bufferView);
}
