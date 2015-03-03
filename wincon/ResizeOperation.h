#pragma once
#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include <vector>
#include <functional>

class ResizeOperation :
	NonCopyable
{
public:
	ResizeOperation(ConsoleHelper& consoleHelper);

	void Start(HWND hWnd, point const& anchor, UINT direction);
	void Finish();

	bool IsActive() const					{ return _isActive; }
	rectangle const& GetRectangle() const	{ return _currentRect; }

	bool HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	template <typename T>
	void OnSizeChanged(T func) { _sizeChangedEvent.push_back(func); }

private:
	void FireSizeChangedEvent();
	void ResizeTo(point const& p);
	void SnapCursorToCell();

private:
	bool		_isActive = false;
	HWND		_hWnd;
	point		_anchor;
	UINT		_direction;
	rectangle	_initialRect;
	rectangle	_currentRect;

	std::vector< std::function<void(void)> > _sizeChangedEvent;

	ConsoleHelper& _consoleHelper;
};
