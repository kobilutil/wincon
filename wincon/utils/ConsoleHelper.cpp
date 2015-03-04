#include "stdafx.h"
#include "ConsoleHelper.h"


void ConsoleHelper::ReOpenHandles()
{
	// TODO: need to add condition on OS version for windows7
	if (_hConOut)
	{
		// HACK: on windows7 there is a bug in conhost.exe that is causing it to crash when 
		// trying to close a hande to a console's screen buffer that was actually created by 
		// another process with a call to CreateConsoleScreenBuffer and was opened here by 
		// calling CreateFile("CONOUT$"..)
		//
		// it is similar to the (mis)behavior that is also described here:
		// Console screen buffer bug in Windows 7 -
		// https://fogbugz.bitvise.com/default.asp?WinSSHD.1.16969.1
		//
		// the workaround is to somewhat guess if we can safely close the handle or just let 
		// it leak.
		//
		// running several tests have shown that those "bad" screen buffers also return incorrect
		// or rather invalid information when calling GetConsoleScreenBufferInfo/GetConsoleFontSize/
		// or GetLargestConsoleWindowSize on them. so based on some senity checks below we deside 
		// if it is safer to just detach from the handle and let it leak or not.

		// NOTE: regarding maxSize, for "bad" handles the maxSize returned seems to be the pixel size
		// of the desktop's working area instead of the max number of cells.
		// TODO: maybe to somehow add this to the sanity checks below.
		size maxSize;
		maxSize << ::GetLargestConsoleWindowSize(_hConOut.get());

		RefreshInfo();
		debug_print("XX: %#x max=%d,%d size=%d,%d font=%d,%d view=%d,%d,%dx%d cursor=%d,%d\n",
			_hConOut.get(), 
			maxSize.width(), maxSize.height(),
			_bufferSize.width(), _bufferSize.height(),
			_cellSize.width(), _cellSize.height(),
			_bufferView.left(), _bufferView.top(),
			_bufferView.width(), _bufferView.height(),
			_caretPos.x(), _caretPos.y());

		if (_cellSize.width() == 0 || _cellSize.height() == 0			// zero font size
			|| _bufferSize.width() <= 0 || _bufferSize.height() <= 0	// negative buffer size
			|| _bufferView.width() > maxSize.width()			// buffer view is bigger than
			|| _bufferView.height() > maxSize.height()			// the maximum allowed
			|| !rectangle(point(), _bufferSize).contains(_caretPos))	// caret is outside the buffer 
		{
			debug_print("ConsoleHelper::ReOpenHandles - conhost crash workaround, leak handle %#x\n", _hConOut.get());
			_hConOut.detach();
		}
	}

	_hConIn.reset(::CreateFile(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
	_hConOut.reset(::CreateFile(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
}

bool ConsoleHelper::RefreshInfo()
{
	::CONSOLE_SCREEN_BUFFER_INFO csbi{};
	if (!::GetConsoleScreenBufferInfo(_hConOut.get(), &csbi))
		return false;

	//auto maxSize = ::GetLargestConsoleWindowSize(_hConOut.get());

	::CONSOLE_FONT_INFO fontInfo{};
	if (!::GetCurrentConsoleFont(_hConOut.get(), FALSE, &fontInfo))
		return false;

	_bufferSize << csbi.dwSize;
	_bufferView << csbi.srWindow;

	//_cellSize << fontInfo.dwFontSize;
	_cellSize << GetConsoleFontSize(_hConOut.get(), fontInfo.nFont);

	_caretPos << csbi.dwCursorPosition;
	return true;
}

bool ConsoleHelper::BufferSize(size const& size)
{
	COORD temp;
	temp << size;
	if (!::SetConsoleScreenBufferSize(_hConOut.get(), temp))
		return false;

	_bufferSize = size;
	return true;
}

bool ConsoleHelper::BufferView(rectangle const& view)
{
	SMALL_RECT rect;
	rect << view;
	if (!::SetConsoleWindowInfo(_hConOut.get(), TRUE, &rect))
		return false;

	_bufferView = view;
	return true;
}

DWORD ConsoleHelper::InputMode() const
{
	DWORD mode = 0;
	::GetConsoleMode(_hConIn.get(), &mode);
	return mode;
}

point ConsoleHelper::MapPixelToCell(point const& p)
{
	return _bufferView.top_left() + p / _cellSize;
}

point ConsoleHelper::MapCellToPixel(point const& p)
{
	return (p - _bufferView.top_left()) * _cellSize;
}

bool ConsoleHelper::WriteInput(::INPUT_RECORD const& inp)
{
	DWORD num = 0;
	if (!::WriteConsoleInput(_hConIn.get(), &inp, 1, &num) || (num != 1))
		return false;

	return true;
}

std::vector<DWORD> ConsoleHelper::GetProcessList()
{
	std::vector<DWORD> pids;

	size_t num = 10;
	while (pids.size() < num)
	{
		pids.resize(num);
		num = ::GetConsoleProcessList(pids.data(), pids.size());
	}
	pids.resize(num);

	return std::move(pids);
}

void ConsoleHelper::Pause()
{
	InternalPauseResume(true);
}

void ConsoleHelper::Resume()
{
	InternalPauseResume(false);
}

void ConsoleHelper::InternalPauseResume(bool doPause)
{
	auto mode = InputMode();

	if ((mode & ENABLE_PROCESSED_INPUT) || (mode & ENABLE_LINE_INPUT))
	{
		::INPUT_RECORD inp{};

		inp.EventType = KEY_EVENT;
		inp.Event.KeyEvent.wVirtualKeyCode = doPause ? VK_PAUSE : VK_ESCAPE;

		inp.Event.KeyEvent.bKeyDown = TRUE;
		WriteInput(inp);

		inp.Event.KeyEvent.bKeyDown = FALSE;
		WriteInput(inp);

		return;
	}

#if 0
	auto currPid = ::GetCurrentProcessId();

	for (auto pid : GetProcessList())
	{
		// dont suspend ourself
		if (pid == currPid)
			continue;

		if (doPause)
			SuspendProcess(pid);
		else
			ResumeProcess(pid);
	}
#endif
}

bool ConsoleHelper::ReadOutput(std::vector<CHAR_INFO>& buffer, rectangle const& region)
{
	buffer.resize(region.width() * region.height());

	COORD bufferSize;
	bufferSize << region.size();

	SMALL_RECT readRect;
	readRect << region;

	auto rc = ::ReadConsoleOutput(_hConOut.get(), buffer.data(), bufferSize, COORD{ 0, 0 }, &readRect);
	return (rc == TRUE);
}

bool ConsoleHelper::IsRegularConsoleWindow(HWND hWnd)
{
	WCHAR name[50];
	if (!::RealGetWindowClass(hWnd, name, ARRAYSIZE(name)))
		return false;

	if (lstrcmp(name, L"ConsoleWindowClass") != 0)
		return false;

	// some processes (like cygwin for example) create a hidden console window for their own purposes
	// we dont't want those

	//if (::IsWindowVisible(hWnd) || ::IsIconic(hWnd))
	if (!::IsWindowVisible(hWnd) && !::IsIconic(hWnd))
		return false;

	return true;
}

DWORD ConsoleHelper::GetRealThreadId()
{
	static DWORD s_consoleRealThreadId;
	static HWND s_consoleHwnd;
	static auto s_winEvent = ::GlobalAddAtom(L"ConsoleHelper::GetRealThreadId-755C9704-B0C6-45C4-B72C-CA37568EBE75");

	auto hWnd = ::GetConsoleWindow();
	if (!hWnd)
		return 0;

	s_consoleHwnd = hWnd;
	s_consoleRealThreadId = 0;

	scoped_wineventhook hHook{ 
		::SetWinEventHook(s_winEvent, s_winEvent, NULL,
			[](HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
			{
				if ((hWnd == s_consoleHwnd) && (s_consoleRealThreadId == 0))
				{
					s_consoleRealThreadId = dwEventThread;
					::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);

					debug_print("ConsoleHelper::GetRealThreadId - found. threadId=%lu\n", s_consoleRealThreadId);
				}
			},
			0, 0, WINEVENT_SKIPOWNPROCESS | WINEVENT_OUTOFCONTEXT
		) 
	};

	if (!hHook)
	{
		debug_print("ConsoleHelper::GetRealThreadId - SetWinEventHook failed. err=%#x\n", ::GetLastError());
		return 0;
	}

	::NotifyWinEvent(s_winEvent, s_consoleHwnd, 0, 0);

	auto hTimer = ::SetTimer(NULL, 0, 5 * 1000, 
		[](HWND, UINT, UINT_PTR, DWORD)
		{
			if (!s_consoleRealThreadId)
			{
				debug_print("ConsoleHelper::GetRealThreadId - timeout occured\n");
				::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
			}
		}
	);

	debug_print("ConsoleHelper::GetRealThreadId - started. consoleHwnd=%lu\n", s_consoleHwnd);

	::MSG msg{};
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	::KillTimer(NULL, hTimer);

	return s_consoleRealThreadId;
}

// default console's signal handler for use with SetConsoleCtrlHandler
static BOOL WINAPI DefaultCtrlHandler(DWORD CtrlType)
{
	switch (CtrlType)
	{
	// ignore ctrl-c and ctrl-break signals
	case CTRL_C_EVENT:
#ifndef _DEBUG 
	// for debug builds allow ctr-break to terminate the app
	case CTRL_BREAK_EVENT:
#endif
		return TRUE;
	}
	// for all other signals allow the default processing.
	// whithout this windows xp will display an ugly end-process dialog box
	// when the user tries to close the console window.
	debug_print("DefaultSignalHandler: %d\n", CtrlType);
	return FALSE;
}

void ConsoleHelper::InstallDefaultCtrlHandler()
{
	::SetConsoleCtrlHandler(DefaultCtrlHandler, TRUE);
}