// wincon.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "wincon.h"
#include "OverlayApp.h"

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
"processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	return OverlayApp::Run();
}