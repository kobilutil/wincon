#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"


class SelectionHelper
{
public:
	SelectionHelper(ConsoleHelper& consoleHelper);
	~SelectionHelper();

	enum Mode
	{
		SELECT_NONE,
		SELECT_CHAR,
		SELECT_WORD,
		SELECT_LINE
	};

	void Start(point const& anchor, enum Mode mode);
	bool ExtendTo(point const& p);
	void Clear();

	bool IsShowing() const		{ return _isShowing; }
	enum Mode Mode() const		{ return _mode; }

	// (p1,p2) represent the selection area
	// the pair is normalized so that (p1 <= p2)
	//
	// by convention, p2.x will be exclusive and p2.y will be inclusive.
	// this means that when selecting forward (mouse > anchor) the cell under the
	// mouse cursor is not selected. 
	// this is similar behaviour to notepad++ and VS.
	// mintty on the other hand treat p2.x as inclusive.

	point const& p1() const		{ return _p1; }
	point const& p2() const		{ return _p2; }

	bool CopyToClipboard(HWND hWndOwner);

	template <typename T>
	void OnSelectionChanged(T func) { _selectionChangedEvent.push_back(func); }

private:
	bool ReadOutputLineToCache(int y);
	void AdjustSelectionAccordingToMode(enum Mode mode, point& p1, point& p2);

	void FireSelectionChangedEvent();

private:
	bool			_isShowing;

	enum Mode		_mode;

	point			_anchorCell;
	point			_p1, _p2;

	std::vector< std::function<void(void)> > _selectionChangedEvent;

	std::vector<CHAR_INFO>	_cachedLine;

	ConsoleHelper&	_consoleHelper;
};
