#include "stdafx.h"
#include "resource.h"

#include "MonitorApp.h"
#include "PerConsoleContext.h"
#include "AboutDialog.h"
#include "utils\ConsoleHelper.h"

#include <functional>
using std::make_shared;

//#include <Commctrl.h>
//#pragma comment(lib, "Commctrl.lib")


MonitorApp::MonitorApp() :
	_singleInstance(L"wincon-MonitorApp-singleton-mutex-134A98C0-8FB7-4044-BE10-B669E878738C")
{}

MonitorApp::~MonitorApp()
{
	_trayIcon.Remove();

	::DestroyWindow(_hMainWnd);
}

int MonitorApp::Run()
{
	MonitorApp app;
	if (!app.Init())
		return -1;

	::MSG msg{};
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	auto rc = (int)msg.wParam;
	return rc;
}

bool MonitorApp::Init()
{
	if (_singleInstance.IsRunning())
		return false;

	::WNDCLASSEX wcex{};
	wcex.cbSize = sizeof(wcex);
	wcex.hInstance = ::GetModuleHandle(NULL);
	wcex.lpszClassName = L"wincon-MonitorApp-HWND_MESSAGE-55023123-AAC7-45B8-9254-14EE6DBF5346";
	wcex.lpfnWndProc = SimpleWindow::Static_WndProc;

	if (!SimpleWindow::RegisterWindowClass(wcex))
		return false;

	_hMainWnd = ::CreateWindowEx(0, wcex.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wcex.hInstance, this);
	if (!_hMainWnd)
		return false;

	PostMessage(_hMainWnd, WM_NULL, 1, 2);

	auto hIcon = (HICON)::LoadImage(::GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_WINCON), IMAGE_ICON, 16, 16, 0);
	_trayIcon.Create(_hMainWnd, hIcon);

	SetupWinEventHooks();

	StartHelpersForExistingConsoles();

	RefreshTrayIconTooltip();

	return true;
}

void MonitorApp::SetupWinEventHooks()
{
	_winEventThunk.reset(
		[this](HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
		{
			switch (event)
			{
			case EVENT_CONSOLE_START_APPLICATION:
				//debug_print("ConsoleCreationMonitor: EVENT_CONSOLE_START_APPLICATION - %lu %lu\n", idObject, idChild);
				OnWinEvent_ConsoleApplicationStarted(hWnd, idObject);
				break;
			case EVENT_CONSOLE_END_APPLICATION:
				//debug_print("ConsoleCreationMonitor: EVENT_CONSOLE_END_APPLICATION - %lu %lu\n", idObject, idChild);
				OnWinEvent_ConsoleApplicationFinished(hWnd, idObject);
				break;
			}
		}
	);

	_winEventHook.reset(
		::SetWinEventHook(
			EVENT_CONSOLE_START_APPLICATION, EVENT_CONSOLE_END_APPLICATION, 
			nullptr, _winEventThunk.get(),
			0, 0, 
			WINEVENT_SKIPOWNPROCESS | WINEVENT_OUTOFCONTEXT)
	);
}

void MonitorApp::StartHelpersForExistingConsoles()
{
	EnumWindows([this](HWND hWnd) -> BOOL
	{
		if (!ConsoleHelper::IsRegularConsoleWindow(hWnd))
			return TRUE;

		auto consoleHwnd = hWnd;

		DWORD consoleOwnerPid = 0;
		::GetWindowThreadProcessId(consoleHwnd, &consoleOwnerPid);
		if (!consoleOwnerPid)
			return TRUE;

		if (!::AttachConsole(consoleOwnerPid))
			return TRUE;

		// get all the processes that are currently connected to the console
		auto pids = ConsoleHelper::GetProcessList();
		::FreeConsole();

		auto ctx = make_shared<PerConsoleContext>(consoleHwnd);
		_consoles[consoleHwnd] = ctx;

		for (auto pid : pids)
			ctx->AttachProcess(pid);

		ctx->LaunchHelper();

		return TRUE;
	});
}

void MonitorApp::OnWinEvent_ConsoleApplicationStarted(HWND consoleHwnd, DWORD pid)
{
	auto it = _consoles.find(consoleHwnd);
	if (it != _consoles.end())
	{
		auto& ctx = it->second;
		ctx->AttachProcess(pid);
		return;
	}

	// ignore console windows that are created hidden
	if (!ConsoleHelper::IsRegularConsoleWindow(consoleHwnd))
		return;

	debug_print("ConsolesMonitorApp: begin monitoring console - hwnd=%lu, ownerpid=%lu\n", consoleHwnd, pid);

	auto ctx = make_shared<PerConsoleContext>(consoleHwnd);
	_consoles[consoleHwnd] = ctx;

	ctx->AttachProcess(pid);
	ctx->LaunchHelper();

	RefreshTrayIconTooltip();
}

void MonitorApp::OnWinEvent_ConsoleApplicationFinished(HWND consoleHwnd, DWORD pid)
{
	auto it = _consoles.find(consoleHwnd);
	if (it != _consoles.end())
	{
		auto& ctx = it->second;

		ctx->DetachProcess(pid);

		if (!ctx->AreProcessesAttached())
		{
			debug_print("ConsolesMonitorApp: end monitoring console - hwnd=%lu\n", consoleHwnd);
			_consoles.erase(it);
			RefreshTrayIconTooltip();
		}
		return;
	}
}

void MonitorApp::RefreshTrayIconTooltip()
{
	WCHAR info[100] = {};
	::wsprintfW(info, L"%u consoles", _consoles.size());
	_trayIcon.ChangeTooltip(info);
}

void MonitorApp::ShowTrayIconMenu()
{
	POINT mousePos = {};
	::GetCursorPos(&mousePos);

	scoped_menu menu{ ::CreatePopupMenu() };
	if (!menu)
		return;

	::AppendMenu(menu.get(), MF_STRING, IDM_ABOUT, L"&About ...");
	::AppendMenu(menu.get(), MF_STRING, IDM_EXIT, L"E&xit");

	::SetForegroundWindow(_hMainWnd);
	int cmd = ::TrackPopupMenu(menu.get(), TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
						mousePos.x, mousePos.y, 0, _hMainWnd, nullptr);
	::SendMessage(_hMainWnd, WM_NULL, 0, 0);

	switch (cmd)
	{
	case IDM_ABOUT:
		OnCmd_About();
		break;
	case IDM_EXIT:
		OnCmd_Exit();
		break;
	}
}

void MonitorApp::OnCmd_About()
{
	AboutDialog::ShowModal(_hMainWnd);
}

void MonitorApp::OnCmd_Exit()
{
	::PostQuitMessage(0);
}

LRESULT MonitorApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (_trayIcon.ProcessMessage(message, wParam))
	{
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			//ShowWindow(hWnd, SW_RESTORE);
			::MessageBeep(MB_OK);
			break;
		case WM_RBUTTONUP:
		//case WM_RBUTTONDOWN:
		//case WM_CONTEXTMENU:
			ShowTrayIconMenu();
		}
		return 0;
	}

	switch (message)
	{
	case WM_CREATE:
		break;
	case WM_DESTROY:
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
