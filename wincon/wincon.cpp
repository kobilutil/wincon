// wincon.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "wincon.h"
#include "ConsoleOverlayWindow.h"
#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
"processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static const auto OPTION_DEFAULT_CMDLINE = L"%COMSPEC%";


DWORD RunProcess(LPCWSTR cmdLine)
{
	PROCESS_INFORMATION pi{};
	STARTUPINFO si{};
	si.cb = sizeof(si);

	// CreateProcess requires a modifiable commandline parameter
	WCHAR cmdLine2[MAX_PATH];
	::ExpandEnvironmentStrings(cmdLine, cmdLine2, ARRAYSIZE(cmdLine2));

	{
		// launching the target process while the Wow64 filesystem redirection is disabled will cause a 64bit Windows 
		// to always choose the 64bit cmd.exe from C:\Windows\system32 instead of the 32bit one from C:\Windows\SysWOW64
		ScopedDisableWow64FsRedirection a;

		if (!::CreateProcess(NULL, cmdLine2, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		{
			debug_print("CreateProcess failed. err=%#x, cmdline=%S\n", ::GetLastError(), cmdLine);
			return 0;
		}
	}

	::CloseHandle(pi.hProcess);
	::CloseHandle(pi.hThread);

	return pi.dwProcessId;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	debug_print("try attaching to parent process's console\n");

	::Sleep(100);
	if (!::AttachConsole(ATTACH_PARENT_PROCESS))
	{
		debug_print("AttachConsole failed. err=%#x", ::GetLastError());
		debug_print("try attaching to new process console\n");

		auto pid = RunProcess(OPTION_DEFAULT_CMDLINE);
		if (!pid)
			return 1;

		::Sleep(100);
		if (!::AttachConsole(pid))
		{
			debug_print("AttachConsole failed. err=%#x", ::GetLastError());
			return 1;
		}
	}

	SingleInstanceApp app(L"wincon-overlay-%lu-%s", ::GetConsoleWindow(), L"CE30069A-FA55-41B0-9DD1-DFF37132B6BB");
	if (app.IsRunning())
		return 1;

	ConsoleHelper::InstallDefaultCtrlHandler();

	auto consoleThreadId = ConsoleHelper::GetRealThreadId();

	::InitCommonControls();

	ConsoleOverlayWindow overlay;
	if (!overlay.Create(consoleThreadId))
	{
		debug_print("overlay.Create - failed\n");
		return 1;
	}

	// Main message loop:
	::MSG msg{};
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}