#include "stdafx.h"
#include "SelectionHelper.h"
#include <algorithm>

enum CharCategory
{
	CHAR_CAT_ALPHANUM = 1,
	CHAR_CAT_SPACE,
	CHAR_CAT_OTHER
};

static int GetCharCategory(WCHAR c)
{
	return ::IsCharAlphaNumeric(c) ? CHAR_CAT_ALPHANUM : iswblank(c) ? CHAR_CAT_SPACE : CHAR_CAT_OTHER;
}

static int GetCharCategory(CHAR_INFO const& ci)
{
	return GetCharCategory(ci.Char.UnicodeChar);
}

template <typename T>
static T FindWordBoundaryHelper(T begin, T end)
{
	auto cat = GetCharCategory(*begin);
	return std::find_if(++begin, end, [cat](decltype(*begin) c)
	{
		return cat != GetCharCategory(c);
	});
}

static size_t FindWordLeftBoundary(std::vector<CHAR_INFO> const& line, size_t pos)
{
	auto begin = std::rbegin(line) + (line.size() - pos - 1);
	auto end = std::rend(line);
	auto it = FindWordBoundaryHelper(begin, end);
	return pos - (it - 1 - begin);
}

static size_t FindWordRightBoundary(std::vector<CHAR_INFO> const& line, size_t pos)
{
	auto begin = std::begin(line) + pos;
	auto end = std::end(line);
	auto it = FindWordBoundaryHelper(begin, end);
	return pos + (it - 1 - begin);
}

static size_t FindEOL(std::vector<CHAR_INFO> const& line)
{
	if (GetCharCategory(line.back()) != CHAR_CAT_SPACE)
		return line.size();

	return FindWordLeftBoundary(line, line.size() - 1);
}


SelectionHelper::SelectionHelper(ConsoleHelper& consoleHelper) : 
	_consoleHelper(consoleHelper)
{
	Clear();
}

SelectionHelper::~SelectionHelper()
{}

void SelectionHelper::Start(point const& anchor, enum Mode mode)
{
	Clear();

	_anchorCell = anchor;
	_p1 = _p2 = _anchorCell;
	_mode = mode;

	AdjustSelectionAccordingToMode(_mode, _p1, _p2);

	debug_print("SelectionHelper::Start - mode=%d, anchor=%d,%d\n", _mode, _anchorCell.x(), _anchorCell.y());

	_isShowing = (_p1 != _p2);
	FireSelectionChangedEvent();
}

bool SelectionHelper::ExtendTo(point const& p)
{
	auto mouseCellPos = p;

	auto consoleBufferSize = _consoleHelper.BufferSize();

	if (mouseCellPos.x() < 0)
		mouseCellPos.x() = 0;

	if (mouseCellPos.x() >= consoleBufferSize.width())
		mouseCellPos.x() = consoleBufferSize.width() - 1;

	if (mouseCellPos.y() < 0)
		mouseCellPos.y() = 0;

	if (mouseCellPos.y() >= consoleBufferSize.height())
		mouseCellPos.y() = consoleBufferSize.height() - 1;

	point p1, p2;

	if (mouseCellPos < _anchorCell)
	{
		p1 = mouseCellPos;
		p2 = _anchorCell;
	}
	else
	{
		p1 = _anchorCell;
		p2 = mouseCellPos;
	}

	AdjustSelectionAccordingToMode(_mode, p1, p2);

	if ((p1 == _p1) && (p2 == _p2))
		return false;

	debug_print("SelectionHelper::ExtendTo - mode=%d, %d,%d - %d,%d\n", _mode, _p1.x(), _p1.y(), _p2.x(), _p2.y());

	_p1 = p1;
	_p2 = p2;
	_isShowing = (_p1 != _p2);
	FireSelectionChangedEvent();

	return true;
}

void SelectionHelper::AdjustSelectionAccordingToMode(enum Mode mode, point& p1, point& p2)
{
	switch (mode)
	{
	case SelectionHelper::SELECT_CHAR:
		break;

	case SelectionHelper::SELECT_WORD:
		ReadOutputLineToCache(p1.y());
		p1.x() = FindWordLeftBoundary(_cachedLine, p1.x());

		if (p2.y() != p1.y())
			ReadOutputLineToCache(p2.y());

		p2.x() = FindWordRightBoundary(_cachedLine, p2.x()) + 1;	
		// +1 because p2.x is exclusive and it points to a cell just after the selection.

		// if the user has selected the empty space at the end of the row, 
		// then wrap the selection to the beginning of the next row.
		if ((p2.x() == _cachedLine.size()) && (GetCharCategory(_cachedLine.back()) == CHAR_CAT_SPACE))
		{
			p2.x() = 0;
			p2.y() += 1;
		}

		break;

	case SelectionHelper::SELECT_LINE:
		{
			p1.x() = 0;

			ReadOutputLineToCache(p2.y());
			int eolPos = FindEOL(_cachedLine);

			// if the user has selected the empty space at the end of the row, or the row contains text up
			// untill the end with no space at all, then wrap the selection to the beginning of the next row.
			if ((p2.x() >= eolPos) || (eolPos == _cachedLine.size()))
			{
				p2.x() = 0;
				p2.y() += 1;
			}
			else
				p2.x() = eolPos;

			break;
		}
	}
}

void SelectionHelper::Clear()
{
	_isShowing = false;
	_anchorCell = _p1 = _p2 = point();
	FireSelectionChangedEvent();
}

bool SelectionHelper::CopyToClipboard(HWND hWndOwner)
{
	if (!_isShowing)
		return false;

	if (!::OpenClipboard(hWndOwner))
		return false;

	::EmptyClipboard();

	auto lineWidth = _consoleHelper.BufferSize().width();
	auto linesCount = _p2.y() - _p1.y() + 1;
	auto maxChars = lineWidth * linesCount
		+ 2 * linesCount		// possible end-of-line (\r\n) for each row
		+ 1;					// additional NULL to mark the end of the string

	auto hMem = ::GlobalAlloc(GMEM_MOVEABLE, maxChars * sizeof(WCHAR));
	auto const pMem = (WCHAR*)::GlobalLock(hMem);
	auto pMemIt = pMem;

	for (auto y = _p1.y(); y <= _p2.y(); ++y)
	{
		ReadOutputLineToCache(y);

		// usually copy a row from the begining till EOL
		decltype(y) begin = 0;
		decltype(y) end = FindEOL(_cachedLine);

		// for the first row, copy from the start column of the selection (p1.x) till EOL
		if (y == _p1.y())
			begin = _p1.x();

		// for last row, copy till the end column of the selection (p2.x) or till EOL (whichever closer)
		if (y == _p2.y())
		{
			if (end > _p2.x())
				end = _p2.x();
		}

		// copy the unicode chars to the memmory buffer
		// TODO: there is probably a bug here regarding handling COMMON_LVB_LEADING_BYTE/COMMON_LVB_TRAILING_BYTE
		for (int i = begin; i < end; ++i)
			*(pMemIt++) = _cachedLine[i].Char.UnicodeChar;

		// add also the end-of-line chars
		if (y != _p2.y())
		{
			*(pMemIt++) = '\r';
			*(pMemIt++) = '\n';
		}
	}
	*(pMemIt++) = 0;

	::GlobalUnlock(hMem);

	hMem = ::GlobalReAlloc(hMem, (pMemIt - pMem) * sizeof(WCHAR), GMEM_MOVEABLE);

	::SetClipboardData(CF_UNICODETEXT, hMem);

	::CloseClipboard();

	return true;
}

bool SelectionHelper::ReadOutputLineToCache(int y)
{
	auto region = rectangle(0, y, _consoleHelper.BufferSize().width(), 1);
	return _consoleHelper.ReadOutput(_cachedLine, region);
}

void SelectionHelper::FireSelectionChangedEvent()
{
	for (auto& func : _selectionChangedEvent)
	{
		func();
	}
}
