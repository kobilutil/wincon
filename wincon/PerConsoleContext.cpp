#include "stdafx.h"
#include "PerConsoleContext.h"

#include <Shellapi.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")


PerConsoleContext::PerConsoleContext(HWND consoleHwnd) :
	helperProcess_{},
	consoleHwnd_(consoleHwnd)
{}

PerConsoleContext::~PerConsoleContext()
{
	// TODO: do a much better child's shutdown
	//::PostThreadMessage(helperProcess_.dwThreadId, WM_QUIT, 0, 0);

	::CloseHandle(helperProcess_.hThread);
	::CloseHandle(helperProcess_.hProcess);
}

void PerConsoleContext::LaunchHelper()
{
	auto hitTest = ::SendMessage(consoleHwnd_, WM_NCHITTEST, 0, MAKELPARAM(10, 10));

	::SetWindowPos(consoleHwnd_, NULL, 0, 0, 0, 0,
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
	bool isAdminRequired = (ERROR_ACCESS_DENIED == ::GetLastError());

	WCHAR path[MAX_PATH]{};
	::GetModuleFileNameEx(::GetCurrentProcess(), NULL, path, ARRAYSIZE(path));

	WCHAR cmdline[MAX_PATH]{};
	::wsprintf(cmdline, L"--consoleHwnd %lu", consoleHwnd_);

	SHELLEXECUTEINFO sei{};
	sei.cbSize = sizeof(sei);
	sei.lpVerb = isAdminRequired ? L"runas" : NULL;
	sei.lpFile = path;
	sei.lpParameters = cmdline;
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	if (!::ShellExecuteEx(&sei))
	{
		DWORD err = GetLastError();
		if (err == ERROR_CANCELLED)
		{
		}
	}

	helperProcess_.dwProcessId = ::GetProcessId(sei.hProcess);
	helperProcess_.hProcess = sei.hProcess;
}

void PerConsoleContext::AttachProcess(DWORD pid)
{
	// dont count the main monitor process or the helper process
	if ((pid != ::GetCurrentProcessId()) && (pid != helperProcess_.dwProcessId))
		attachedProcesses_.insert(pid);
}

void PerConsoleContext::DetachProcess(DWORD pid)
{
	attachedProcesses_.erase(pid);
}

bool PerConsoleContext::AreProcessesAttached() const
{
	return attachedProcesses_.size() > 0;
}
