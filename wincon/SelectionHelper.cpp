#include "stdafx.h"
#include "SelectionHelper.h"

enum CharCategory
{
	CHAR_CAT_ALPHANUMERIC = 1,
	CHAR_CAT_SPACE,
	CHAR_CAT_OTHER
};

static int GetCharCategory(WCHAR c)
{
	return ::IsCharAlphaNumeric(c) ? CHAR_CAT_ALPHANUMERIC : isspace(c) ? CHAR_CAT_SPACE : CHAR_CAT_OTHER;
}

template <typename T>
static size_t FindWordHelper(T begin, T end)
{
	auto it = begin;
	auto j = 0;
	int cat = 0;

	while (it != end)
	{
		auto c = it->Char.UnicodeChar;
		auto cat2 = GetCharCategory(c);

		if (cat == 0)
			cat = cat2;
		else
			if (cat != cat2)
				break;
			else
				++j;

		++it;
	}

	return j;
}

static size_t FindWordLeft(std::vector<CHAR_INFO> const& line, size_t pos)
{
	auto offset = FindWordHelper(rbegin(line) + (line.size() - pos - 1), rend(line));
	return pos - offset;
}

static size_t FindWordRight(std::vector<CHAR_INFO> const& line, size_t pos)
{
	auto offset = FindWordHelper(begin(line) + pos, end(line));
	return pos + offset;
}

static size_t FindEOL(std::vector<CHAR_INFO> const& line)
{
	auto lastIndex = line.size() - 1;

	if (GetCharCategory(line[lastIndex].Char.UnicodeChar) != CHAR_CAT_SPACE)
		return lastIndex;

	return FindWordLeft(line, lastIndex);
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

	_isSelecting = true;
	_isShowing = true;
	_anchorCell = anchor;
	_p1 = _p2 = _anchorCell;
	_mode = mode;

	if (mode == SelectionHelper::SELECT_WORD)
	{
		ReadOutputLineToCache(_anchorCell.y());

		_p1.x() = FindWordLeft(_cachedLine, _anchorCell.x());
		_p2.x() = FindWordRight(_cachedLine, _anchorCell.x());
	}
	else if (mode == SelectionHelper::SELECT_LINE)
	{
		ReadOutputLineToCache(_anchorCell.y());

		_p1.x() = 0;
		_p2.x() = FindEOL(_cachedLine);
	}
}

void SelectionHelper::Continue(point const& p)
{
	if (_isShowing)
	{
		if (p > _anchorCell)
			_p2 = p;
		else
			_p1 = p;
	}
	else
	{
		_anchorCell = _p1 = _p2 = p;
		_isShowing = true;
	}
}

bool SelectionHelper::ExtendTo(point const& p)
{
	auto mouseCellPos = p;

	auto consoleBufferWindow = _consoleHelper.BufferView();
	auto consoleBufferWindowPrev = consoleBufferWindow;
	auto consoleBufferSize = _consoleHelper.BufferSize();

	if ((mouseCellPos.y() < consoleBufferWindow.top()) && (consoleBufferWindow.top() > 0))
	{
		mouseCellPos.y() = consoleBufferWindow.top();
		--consoleBufferWindow.top();
	}

	if ((mouseCellPos.y() >= consoleBufferWindow.bottom()) && (consoleBufferWindow.bottom() < consoleBufferSize.height()))
	{
		mouseCellPos.y() = consoleBufferWindow.bottom() - 1;
		++consoleBufferWindow.top();
	}

	// update the console's buffer position
	if (consoleBufferWindow != consoleBufferWindowPrev)
		_consoleHelper.BufferView(consoleBufferWindow);

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

	switch (_mode)
	{
		case SelectionHelper::SELECT_CHAR:
			break;
		case SelectionHelper::SELECT_WORD:
		{
			ReadOutputLineToCache(p1.y());
			p1.x() = FindWordLeft(_cachedLine, p1.x());

			ReadOutputLineToCache(p2.y());
			p2.x() = FindWordRight(_cachedLine, p2.x());

			break;
		}
		case SelectionHelper::SELECT_LINE:
		{
			ReadOutputLineToCache(_p2.y());

			p1.x() = 0;
			p2.x() = FindEOL(_cachedLine);
			break;
		}
	}

	if ((p1 == _p1) && (p2 == _p2))
		return false;

	_p1 = p1;
	_p2 = p2;

	return true;
}

void SelectionHelper::Finish()
{
	_isSelecting = false;
}

void SelectionHelper::Clear()
{
	Finish();
	_isShowing = false;
	_anchorCell = _p1 = _p2 = point();
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
		for (int i = begin; i <= end; ++i)
			*(pMemIt++) = _cachedLine[i].Char.UnicodeChar;

		// add also the end-of-line chars
		if ((y != _p2.y()) || (_mode == SELECT_LINE))
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
