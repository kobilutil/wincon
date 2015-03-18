// tests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "wincon\utils\ConsoleHelper.h"
#include <process.h>

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
		}
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

	ConsoleHelper console;
	console.ReOpenHandles();
	console.RefreshInfo();

	//print(console.hConOut().get(), "hwnd %x\n", ::GetConsoleWindow());

	print(console.hConOut().get(), "1st buffer\n");
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

	print(console.hConOut().get(), "back to 1st buffer\n");
	getchar();
}


int _tmain(int argc, _TCHAR* argv[])
{
	test();
	return 0;
}

