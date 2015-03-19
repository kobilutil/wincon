// tests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "wincon\utils\ConsoleHelper.h"
#include <process.h>

ConsoleHelper _consoleHelper;

void print(HANDLE hFile, char const* format, ...)
{
	va_list args;
	va_start(args, format);
	char line[200];
	auto len = ::wvsprintfA(line, format, args);
	va_end(args);
	DWORD written;
	::WriteFile(hFile, line, len, &written, NULL);
}

//::GetWindowPlacement(hWnd, &wp2);
//debug_print("** show=%d, norm=%d,%d-%d,%d min=%d,%d max=%d,%d\n", wp2.showCmd,
//	wp2.rcNormalPosition.left, wp2.rcNormalPosition.top, wp2.rcNormalPosition.right, wp2.rcNormalPosition.bottom,
//	wp2.ptMinPosition.x, wp2.ptMinPosition.y,
//	wp2.ptMaxPosition.x, wp2.ptMaxPosition.y
//	);


char const* idobject_to_string(LONG idObject)
{
	switch (idObject)
	{
	case OBJID_WINDOW:
		return "window";
	case OBJID_SYSMENU:
		return "sysmenu";
	case OBJID_TITLEBAR:
		return "titlebar";
	case OBJID_MENU:
		return "menu";
	case OBJID_CLIENT:
		return "client";
	case OBJID_VSCROLL:
		return "vscroll";
	case OBJID_HSCROLL:
		return "hscroll";
	case OBJID_SIZEGRIP:
		return "sizegrip";
	case OBJID_CARET:
		return "caret";
	case OBJID_CURSOR:
		return "cursor";
	case OBJID_ALERT:
		return "alert";
	case OBJID_SOUND:
		return "sound";
	case OBJID_QUERYCLASSNAMEIDX:
		return "queryclass";
	case OBJID_NATIVEOM:
		return "oem";
	default:
		return "<unknown>";
	}
}

struct __zz{
	bool	isMoveSize = false;
	bool	isCaptured = false;
	DWORD	captureEventTime = 0;
	bool	isZoomed = false;
	//bool	isTitlebarStateChanged = false;
	DWORD	titlebarStateChangedEventTime = 0;
	size	bufferSize;
	size	bufferViewSize;
	WINDOWPLACEMENT wp{};
	DWORD	scrollbarsStye = 0;

	bool IsUserInitiatedAction(DWORD dwmsEventTime)
	{
		auto dc = ::GetDoubleClickTime();
		return isMoveSize || isCaptured ||
			((dwmsEventTime - captureEventTime) < dc) ||
			((dwmsEventTime - titlebarStateChangedEventTime) < dc);
	}

	void Process(DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwmsEventTime)
	{
		if (hWnd != ::GetConsoleWindow())
			return;

		switch (event)
		{
		case EVENT_CONSOLE_LAYOUT:
			CheckZoomState(hWnd, dwmsEventTime);
			break;
		case EVENT_OBJECT_REORDER:
			if (!isMoveSize && (idObject == OBJID_WINDOW) && (idChild == CHILDID_SELF))
			{
				_consoleHelper.ReOpenHandles();
				_consoleHelper.RefreshInfo();
			}
			break;
		case EVENT_SYSTEM_MOVESIZESTART:
			this->isMoveSize = true;
			break;
		case EVENT_SYSTEM_MOVESIZEEND:
			this->isMoveSize = false;
			break;
		case EVENT_SYSTEM_CAPTURESTART:
			this->isCaptured = true;
			this->captureEventTime = dwmsEventTime;
			break;
		case EVENT_SYSTEM_CAPTUREEND:
			this->isCaptured = false;
			this->captureEventTime = dwmsEventTime;
			break;
		//case EVENT_SYSTEM_MINIMIZESTART:
		//	_consoleHelper.ReOpenHandles();
		//	_consoleHelper.RefreshInfo();
		//	break;
		case EVENT_SYSTEM_MINIMIZEEND:
			RestoreFromMinimizedFixup(hWnd);
			break;
		case EVENT_OBJECT_STATECHANGE:
			if (idObject == OBJID_TITLEBAR)
				titlebarStateChangedEventTime = dwmsEventTime;
			break;
		}
	}

	void RestoreFromMinimizedFixup(HWND hWnd)
	{
		auto isZoomed = (IsZoomed(hWnd) == TRUE);
		if (isZoomed && (isZoomed == this->isZoomed))
		{
			_consoleHelper.ReOpenHandles();
			_consoleHelper.RefreshInfo();
			_consoleHelper.Resize(_consoleHelper.LargestViewSize());
		}
	}

	void CheckZoomState(HWND hWnd, DWORD dwmsEventTime)
	{
		//if (this->isMoveSize)
		//	return;

		auto isZoomed = (IsZoomed(hWnd) == TRUE);
		if (isZoomed != this->isZoomed)
		{
			debug_print("** zoom state changed - %s\n", isZoomed ? "zoomed" : "normal");

			if (isZoomed && !this->isZoomed)
			{
				this->bufferSize = _consoleHelper.BufferSize();
				this->bufferViewSize = _consoleHelper.BufferView().size();

				this->wp.length = sizeof(this->wp);
				::GetWindowPlacement(hWnd, &this->wp);

				this->scrollbarsStye = ::GetWindowLongPtr(hWnd, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL);

				_consoleHelper.ReOpenHandles();
				_consoleHelper.RefreshInfo();

				_consoleHelper.Resize(_consoleHelper.LargestViewSize());

				this->isZoomed = true;
			}
			else if (!isZoomed && this->isZoomed)
			{
				_consoleHelper.ReOpenHandles();
				_consoleHelper.RefreshInfo();

				if (!IsUserInitiatedAction(dwmsEventTime))
				{
					auto wp2 = this->wp;
					wp2.flags = 0;
					wp2.showCmd = SW_MAXIMIZE;
					::SetWindowPlacement(hWnd, &wp2);

					_consoleHelper.RefreshInfo();
					_consoleHelper.Resize(_consoleHelper.LargestViewSize());

					this->scrollbarsStye = ::GetWindowLongPtr(hWnd, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL);
				}
				else
				{
					_consoleHelper.Resize(this->bufferViewSize);

					auto currScrollbarsStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL);
					if (currScrollbarsStyle != this->scrollbarsStye)
					{
						if ((this->scrollbarsStye & WS_VSCROLL) == 0)
							_consoleHelper.BufferSize(this->bufferViewSize);
					}

					this->isZoomed = false;
				}
			}
		}
	}
}
s_consoleState;

void SetupWinEventHooks(DWORD _consoleWindowThreadId)
{
	static auto s_hWndConsole = ::GetConsoleWindow();
	static StaticThunk<WINEVENTPROC> _winEventThunk;
	_winEventThunk.reset(
		[](HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
	{
		switch (event)
		{
		case EVENT_CONSOLE_CARET:
			debug_print("CONSOLE_CARET - %d,%d %ld\n", LOWORD(idChild), HIWORD(idChild), idObject);
			break;
		case EVENT_CONSOLE_UPDATE_REGION:
			debug_print("CONSOLE_UPDATE_REGION - %d,%d %d,%d\n", LOWORD(idObject), HIWORD(idObject), LOWORD(idChild), HIWORD(idChild));
			break;
		case EVENT_CONSOLE_UPDATE_SIMPLE:
			debug_print("CONSOLE_UPDATE_SIMPLE - %d,%d '%C' %d\n", LOWORD(idObject), HIWORD(idObject), LOWORD(idChild), HIWORD(idChild));
			break;
		case EVENT_CONSOLE_UPDATE_SCROLL:
			debug_print("CONSOLE_SCROLL - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_CONSOLE_LAYOUT:
			debug_print("CONSOLE_LAYOUT - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_CONSOLE_START_APPLICATION:
			debug_print("START_APPLICATION - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_CONSOLE_END_APPLICATION:
			debug_print("END_APPLICATION - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_MOVESIZESTART:
			debug_print("MOVESIZESTART - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_MOVESIZEEND:
			debug_print("MOVESIZEEND - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_MINIMIZESTART:
			debug_print("MINIMIZESTART - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_MINIMIZEEND:
			debug_print("MINIMIZEEND - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_SYSTEM_FOREGROUND:
			debug_print("FOREGROUND - %ld %ld\n", idObject, idChild);
			break;
		case EVENT_OBJECT_REORDER:
			//if (!idObject && !idChild)
			debug_print("OBJECT_REORDER - %s(%ld) %ld\n", idobject_to_string(idObject), idObject, idChild);
			break;
		case EVENT_OBJECT_LOCATIONCHANGE:
			
			if ((idObject == OBJID_CURSOR) || (idObject == OBJID_CARET))
				break;

			debug_print("LOCATIONCHANGE - %s(%ld) %ld\n", idobject_to_string(idObject), idObject, idChild);
			break;
		case EVENT_OBJECT_STATECHANGE:
			debug_print("OBJECT_STATECHANGE - %s(%ld) %ld\n", idobject_to_string(idObject), idObject, idChild);
			break;
		case EVENT_SYSTEM_CAPTURESTART:
			debug_print("CAPTURESTART - %s(%ld) %ld\n", idobject_to_string(idObject), idObject, idChild);
			break;
		case EVENT_SYSTEM_CAPTUREEND:
			debug_print("CAPTUREEND - %s(%ld) %ld\n", idobject_to_string(idObject), idObject, idChild);
			break;
		}

		s_consoleState.Process(event, hWnd, idObject, idChild, dwmsEventTime);
	});

	auto helper = [&_consoleWindowThreadId](DWORD eventMin, DWORD eventMax)
	{
		return ::SetWinEventHook(eventMin, eventMax, NULL, _winEventThunk.get(), 0, _consoleWindowThreadId, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	};

	auto hHooks{
		helper(EVENT_CONSOLE_CARET, EVENT_CONSOLE_END_APPLICATION),
		helper(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND),
		helper(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND),
		helper(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND),
		helper(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE),
		helper(EVENT_OBJECT_REORDER, EVENT_OBJECT_REORDER),
		helper(EVENT_OBJECT_STATECHANGE, EVENT_OBJECT_STATECHANGE),
		helper(EVENT_SYSTEM_CAPTURESTART, EVENT_SYSTEM_CAPTUREEND),
	};

	static std::vector<scoped_wineventhook> _winEventHooks;
	for (auto h : hHooks)
		_winEventHooks.push_back(scoped_wineventhook(h));
}

void test()
{
	_beginthread([](LPVOID)
	{
		SetupWinEventHooks(ConsoleHelper::GetRealThreadId());
		::MSG msg{};
		while (::GetMessage(&msg, NULL, 0, 0))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}
	, 0, nullptr);

	Sleep(1000);

	_consoleHelper.ReOpenHandles();
	_consoleHelper.RefreshInfo();

	//print(console.hConOut().get(), "hwnd %x\n", ::GetConsoleWindow());

	print(_consoleHelper.hConOut().get(), "1st buffer\n");
	getchar();

	{
		scoped_handle out2{
			::CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL)
		};

		scoped_handle out3{
			::CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL)
		};

		::SetConsoleActiveScreenBuffer(out2.get());

		print(out2.get(), "2nd buffer\n");
		getchar();

		::SetConsoleActiveScreenBuffer(out3.get());

		print(out3.get(), "3rd buffer\n");
		getchar();
	}

	::SetConsoleActiveScreenBuffer(::GetStdHandle(STD_OUTPUT_HANDLE));
	print(::GetStdHandle(STD_OUTPUT_HANDLE), "back to 1st buffer\n");
	getchar();
}


int _tmain(int argc, _TCHAR* argv[])
{
	test();
	return 0;
}

