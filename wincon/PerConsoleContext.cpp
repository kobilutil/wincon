#include "stdafx.h"
#include "PerConsoleContext.h"

#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")


PerConsoleContext::PerConsoleContext(HWND consoleHwnd) :
	helperProcess_{},
	consoleHwnd_(consoleHwnd)
{}

PerConsoleContext::~PerConsoleContext()
{
	// TODO: do a much better child's shutdown
	::PostThreadMessage(helperProcess_.dwThreadId, WM_QUIT, 0, 0);

	::CloseHandle(helperProcess_.hThread);
	::CloseHandle(helperProcess_.hProcess);
}

void PerConsoleContext::LaunchHelper()
{
	WCHAR path[MAX_PATH] = { 0 };
	::GetModuleFileNameEx(::GetCurrentProcess(), nullptr, path, ARRAYSIZE(path));

	WCHAR cmdline[MAX_PATH] = {};
	wsprintf(cmdline, L"\"%s\" --consoleHwnd %lu", path, consoleHwnd_);

	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);

	::CreateProcess(path, cmdline, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

	helperProcess_ = pi;
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
