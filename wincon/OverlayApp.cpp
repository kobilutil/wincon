#include "stdafx.h"
#include "OverlayApp.h"
#include "ConsoleOverlayWindow.h"
#include "utils\ConsoleHelper.h"

#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

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

int OverlayApp::Run(HWND hWndConsole)
{
	DWORD pid = 0;
	::GetWindowThreadProcessId(hWndConsole, &pid);
	if (!pid)
		return 1;

	debug_print("attaching to process's console. pid=%ld, hWnd=%#x\n", pid, hWndConsole);

	if (!::AttachConsole(pid))
	{
		debug_print("AttachConsole failed. err=%#x", ::GetLastError());
		return 1;
	}

	ConsoleHelper::InstallDefaultCtrlHandler();

	SingleInstanceApp app(L"wincon-overlay-%lu-%s", ::GetConsoleWindow(), L"CE30069A-FA55-41B0-9DD1-DFF37132B6BB");
	if (app.IsRunning())
		return 1;

	auto consoleThreadId = ConsoleHelper::GetRealThreadId();
	if (!consoleThreadId)
		return 1;

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