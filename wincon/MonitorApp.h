#pragma once

#include <memory>
#include <hash_map>

#include "utils\utils.h"
#include "utils\TrayIcon.h"


class PerConsoleContext;

class MonitorApp
{
	using SimpleWindow = SimpleWndProc < MonitorApp >;
	friend SimpleWindow;

	MonitorApp();
	~MonitorApp();

public:
	static int Run();

private:
	bool Init();

	void SetupWinEventHooks();
	void StartHelpersForExistingConsoles();

	void OnWinEvent_ConsoleApplicationStarted(HWND consoleHwnd, DWORD pid);
	void OnWinEvent_ConsoleApplicationFinished(HWND consoleHwnd, DWORD pid);

	void RefreshTrayIconTooltip();
	void ShowTrayIconMenu();

	void OnCmd_About();
	void OnCmd_Exit();

	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	SingleInstanceApp	_singleInstance;

	HWND			_hMainWnd;
	TrayIcon		_trayIcon;

	StaticThunk<WINEVENTPROC, MonitorApp>	_winEventThunk;
	scoped_wineventhook						_winEventHook;

	typedef std::shared_ptr<PerConsoleContext>	ContextPtr;
	typedef std::hash_map<HWND, ContextPtr>		ConsolesContext;

	ConsolesContext		_consoles;
};

