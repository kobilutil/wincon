#include "stdafx.h"
#include "utils.h"

#include <stdio.h>
#include <TlHelp32.h>

//
// a helper macro that uses c++ compile-time type detection to ease the usage of the
// GetProcAddress api. For example, compare
//
// this:
//		typedef BOOL(WINAPI *LPFN_Wow64DisableWow64FsRedirection) (PVOID *OldValue);
//		static auto func = (LPFN_Wow64DisableWow64FsRedirection)::GetProcAddress(::GetModuleHandle(L"kernel32"), "Wow64DisableWow64FsRedirection");
//
// to that:
//		static auto func = GETPROC(Wow64DisableWow64FsRedirection, kernel32);
//
#define GETPROC(F,M) (decltype(&F))::GetProcAddress(::GetModuleHandle(L#M), #F)

bool InternalSuspendResumeProcess(DWORD processId, bool suspend);

class vargs : NonCopyable
{
public:
	template <typename T>
	inline vargs(T& format)
	{
		va_start(_args, format);
	}
	~vargs()
	{
		va_end(_args);
	}
	va_list get() { return _args; }
private:
	va_list _args;
};

void debug_print(char const* format, ...)
{
#ifdef _DEBUG
	vargs args(format);

	char line[MAX_PATH];
	::wvsprintfA(line, format, args.get());

	::OutputDebugStringA(line);
#endif
}

SingleInstanceApp::SingleInstanceApp(LPCWSTR appName, ...)
{
	vargs args(appName);

	WCHAR fullname[MAX_PATH];
	::wvsprintf(fullname, appName, args.get());

	_markingMutex.reset(::CreateMutex(nullptr, FALSE, fullname));
	_isRunning = (::GetLastError() == ERROR_ALREADY_EXISTS);
}

ScopedDisableWow64FsRedirection::ScopedDisableWow64FsRedirection()
{
	if (IsWow64())
		_redirected = DisableWow64FsRedirection(&_ctx);
}

ScopedDisableWow64FsRedirection::~ScopedDisableWow64FsRedirection()
{
	if (_redirected)
		RevertWow64FsRedirection(_ctx);
}

bool ScopedDisableWow64FsRedirection::IsWow64()
{
	static auto func = GETPROC(IsWow64Process, kernel32);
	if (!func)
		return false;

	BOOL bIsWow64;
	if (!func(::GetCurrentProcess(), &bIsWow64))
		return false;

	return bIsWow64 == TRUE;
}

bool ScopedDisableWow64FsRedirection::DisableWow64FsRedirection(PVOID* ctx)
{
	static auto func = GETPROC(Wow64DisableWow64FsRedirection, kernel32);
	if (!func)
		return false;

	return func(ctx) == TRUE;
}

bool ScopedDisableWow64FsRedirection::RevertWow64FsRedirection(PVOID ctx)
{
	static auto func = GETPROC(Wow64RevertWow64FsRedirection, kernel32);
	if (!func)
		return false;

	return func(ctx) == TRUE;
}

bool SuspendProcess(DWORD pid)
{
	return InternalSuspendResumeProcess(pid, true);
}

bool ResumeProcess(DWORD pid)
{
	return InternalSuspendResumeProcess(pid, false);
}

static bool InternalSuspendResumeProcess(DWORD pid, bool suspend)
{
	// http://stackoverflow.com/questions/11010165/how-to-suspend-resume-a-process-in-windows

	LONG NTAPI NtSuspendProcess(IN HANDLE h);

	static auto ntfunc = GETPROC(NtSuspendProcess, ntdll);
	if (ntfunc)
	{
		scoped_handle hProcess{ ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid) };
		if (hProcess)
			if (ntfunc(hProcess.get()) == 0)	// STATUS_SUCCESS = 0
				return true;
	}

	scoped_handle hSnapshot{ ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0) };
	if (!hSnapshot)
		return false;

	::THREADENTRY32 entry{ sizeof(entry) };
	if (!::Thread32First(hSnapshot.get(), &entry))
		return false;

	do
	{
		if (entry.th32OwnerProcessID == pid)
		{
			scoped_handle hThread{ ::OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID) };
			if (suspend)
				::SuspendThread(hThread.get());
			else
				::ResumeThread(hThread.get());

			return ::GetLastError() == ERROR_SUCCESS;
		}
	} 
	while (::Thread32Next(hSnapshot.get(), &entry));

	return false;
}

rectangle GetWindowRect(HWND hWnd)
{
	RECT temp;
	::GetWindowRect(hWnd, &temp);

	rectangle r;
	r << temp;

	return r;
}

rectangle GetClientRect(HWND hWnd)
{
	RECT temp;
	::GetClientRect(hWnd, &temp);

	rectangle r;
	r << temp;

	return r;
}

rectangle MapWindowPoints(HWND hWndFrom, HWND hWndTo, rectangle const& r)
{
	RECT temp;
	temp << r;

	::MapWindowPoints(hWndFrom, hWndTo, (LPPOINT)&temp, 2);

	rectangle r2;
	r2 << temp;

	return r2;
}

point MapWindowPoints(HWND hWndFrom, HWND hWndTo, point const& p)
{
	POINT temp;
	temp << p;

	::MapWindowPoints(hWndFrom, hWndTo, &temp, 1);

	point p2;
	p2 << temp;

	return p2;
}

point GetCursorPos()
{
	POINT p;
	::GetCursorPos(&p);

	point p2;
	p2 << p;

	return p2;
}

bool SetCursorPos(point const& p)
{
	return ::SetCursorPos(p.x(), p.y()) == TRUE;
}

int CutoffRegion(scoped_gdi_region const& source, rectangle const& rect)
{
	RECT rect2;
	rect2 << rect;

	scoped_gdi_region rgn{ ::CreateRectRgnIndirect(&rect2) };

	return ::CombineRgn(source.get(), source.get(), rgn.get(), RGN_DIFF);
};

void CenterWindow(HWND hWnd)
{
	auto rectWnd = GetWindowRect(hWnd);
	auto rectDesktop = GetWindowRect(::GetDesktopWindow());

	auto p = point(0, 0) + rectDesktop.size() / size(2, 2) - rectWnd.size() / size(2, 2);
	::SetWindowPos(hWnd, NULL, p.x(), p.y(), 0, 0,
		SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
}
