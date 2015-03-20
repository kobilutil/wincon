#include "stdafx.h"
#include "ResizeOperation.h"

ResizeOperation::ResizeOperation(ConsoleHelper& consoleHelper) :
	_consoleHelper(consoleHelper)
{}

void ResizeOperation::Start(HWND hWnd, point const& anchor, UINT direction)
{
	if (IsActive())
		Finish();

	debug_print("ResizeOperation::Start - anchor=(%d,%d), dir=%d\n",
		anchor.x(), anchor.y(), direction);

	_hWnd = hWnd;
	_anchor = anchor;
	_direction = direction;
	_initialRect = GetWindowRect(::GetConsoleWindow());
	_currentRect = _initialRect;
	_isActive = true;

	::SetCapture(_hWnd);
}

void ResizeOperation::Finish()
{
	if (!IsActive())
		return;

	debug_print("ResizeOperation::Finish\n");

	_hWnd = NULL;
	_isActive = false;
	::ReleaseCapture();

	SnapCursorToCell();

	FireSizeChangedEvent();
}

bool ResizeOperation::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

void ResizeOperation::FireSizeChangedEvent()
{
	for (auto& func : _sizeChangedEvent)
		func();
}

void ResizeOperation::ResizeTo(point const& p)
{
	_currentRect = _initialRect;

	switch (_direction)
	{
	case HTLEFT:
		_currentRect.left() += (p.x() - _anchor.x());
		_currentRect.size().width() -= (p.x() - _anchor.x());
		break;
	case HTRIGHT:
		_currentRect.size().width() += (p.x() - _anchor.x());
		break;
	case HTTOP:
		_currentRect.top() += (p.y() - _anchor.y());
		_currentRect.size().height() -= (p.y() - _anchor.y());
		break;
	case HTTOPLEFT:
		_currentRect.top() += (p.y() - _anchor.y());
		_currentRect.left() += (p.x() - _anchor.x());
		_currentRect.size().width() -= (p.x() - _anchor.x());
		_currentRect.size().height() -= (p.y() - _anchor.y());
		break;
	case HTTOPRIGHT:
		_currentRect.top() += (p.y() - _anchor.y());
		_currentRect.size().height() -= (p.y() - _anchor.y());
		_currentRect.size().width() += (p.x() - _anchor.x());
		break;
	case HTBOTTOM:
		_currentRect.size().height() += (p.y() - _anchor.y());
		break;
	case HTBOTTOMLEFT:
		_currentRect.left() += (p.x() - _anchor.x());
		_currentRect.size().width() -= (p.x() - _anchor.x());
		_currentRect.size().height() += (p.y() - _anchor.y());
		break;
	case HTBOTTOMRIGHT:
		_currentRect.size().width() += (p.x() - _anchor.x());
		_currentRect.size().height() += (p.y() - _anchor.y());
		break;
	}

	FireSizeChangedEvent();
}

void ResizeOperation::SnapCursorToCell()
{
	auto delta = GetCursorPos() - _anchor;

	auto cellSize = _consoleHelper.CellSize();
	delta = (delta / cellSize) * cellSize;

	SetCursorPos(_anchor + delta);
}
