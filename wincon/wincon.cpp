// wincon.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "wincon.h"
#include "MonitorApp.h"
#include "OverlayApp.h"
#include <shellapi.h>

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
"processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	auto cmdline = GetCommandLineW();

	int argc = 0;
	auto argv = ::CommandLineToArgvW(cmdline, &argc);

	if (argc <= 1)
		return MonitorApp::Run();
	else
	{
		if (wcscmp(argv[1], L"--consoleHwnd") == 0)
		{
			auto consoleHwnd = reinterpret_cast<HWND>(_wtol(argv[2]));
			return OverlayApp::Run(consoleHwnd);
		}
	}
	return 1;
}