#pragma once

#include <hash_set>


class PerConsoleContext
{
	PerConsoleContext(PerConsoleContext const&) = delete;
	PerConsoleContext& operator=(PerConsoleContext const&) = delete;

public:
	PerConsoleContext(HWND consoleHwnd);
	~PerConsoleContext();

	void LaunchHelper();

	void AttachProcess(DWORD pid);
	void DetachProcess(DWORD pid);

	bool AreProcessesAttached() const;

private:
	typedef std::hash_set<DWORD> ProcessPids;

	HWND			consoleHwnd_;
	ProcessPids		attachedProcesses_;
	PROCESS_INFORMATION helperProcess_;
};
