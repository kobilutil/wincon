#pragma once

#include <functional>
#include "scoped_resource.h"
#include "scoped_helpers.h"
#include "rectangle.h"
#include "rectangle_helpers.h"


struct NonCopyable 
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable & operator=(const NonCopyable&) = delete;
};

template <typename T>
struct SimpleWndProc
{
	static LRESULT CALLBACK Static_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		T* pThis = nullptr;

		if (uMsg == WM_NCCREATE)
		{
			auto lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			pThis = reinterpret_cast<T*>(lpcs->lpCreateParams);
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
		}
		else
			pThis = reinterpret_cast<T*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

		if (!pThis)
			return ::DefWindowProc(hWnd, uMsg, wParam, lParam);

		return pThis->WndProc(hWnd, uMsg, wParam, lParam);
	}

	static bool RegisterWindowClass(::WNDCLASSEX const& wcex)
	{
		static ATOM s_atom{};
		static HINSTANCE s_hInstance{};

		if (!s_atom)
		{
			auto atom = ::RegisterClassEx(&wcex);
			if (!atom)
				return false;

			s_atom = atom;
			s_hInstance = wcex.hInstance;

			::atexit([](){
				::UnregisterClass(MAKEINTATOM(s_atom), s_hInstance);
			});
		}
		return true;
	}

	static void detach(HWND hWnd)
	{
		if (hWnd)
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
	}
};

template <typename T>
struct SimpleDlgProc
{
	static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		T *pThis = nullptr;

		if (uMsg == WM_INITDIALOG)
		{
			pThis = reinterpret_cast<T*>(lParam);
			::SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
		}
		else
			pThis = reinterpret_cast<T*>(::GetWindowLongPtr(hDlg, DWLP_USER));

		if (!pThis)
			return FALSE;

		return pThis->DlgProc(hDlg, uMsg, wParam, lParam);
	}

	static void detach(HWND hDlg)
	{
		::SetWindowLongPtr(hDlg, DWLP_USER, 0);
	}
};

template <typename T>
class SimpleSingleton :
	NonCopyable
{
protected:
	SimpleSingleton()
	{}

public:
	static T& Instance()
	{
		static T instance;
		return instance;
	}
};

class SingleInstanceApp
{
public:
	SingleInstanceApp(LPCWSTR appName, ...);
	~SingleInstanceApp() = default;

	bool IsRunning() const	{ return _isRunning; }

private:
	scoped_handle	_markingMutex;
	bool			_isRunning;
};

template <typename S, typename Tag = void>
class StaticThunk :
	NonCopyable
{
	using Signature = typename std::remove_reference< typename std::remove_pointer<S>::type >::type;

public:
	StaticThunk() = default;
	~StaticThunk()
	{
		reset();
	}

	template <typename L>
	void reset(L func)
	{
		auto& this_ref = get_this_ref();

		// abort if the static instance pointer is already set and not by this instance
		if (this_ref && (this_ref != this))
			return;

		this_ref = this;

		_func = func;
	}

	void reset()
	{
		if (this == get_this_ref())
			get_this_ref() = nullptr;
	}

	explicit operator bool() const
	{
		auto& this_ref = get_this_ref();
		return (this_ref != nullptr) && (this_ref == this);
	}

	S get() const
	{
		return static_cast<S>(static_proc);
	}

private:
	std::function<Signature>	_func;

	static StaticThunk*& get_this_ref()
	{
		static StaticThunk* s_this = nullptr;
		return s_this;
	}

	template<typename R, typename... Args>
	static R CALLBACK static_proc(Args... args)
	{
		auto& this_ref = get_this_ref();
		if (this_ref && this_ref->_func)
			return this_ref->_func(args...);

		return R();
	}
};

// temporary disable the Wow64 file system redirection for 32bit process under 64bit OS
class ScopedDisableWow64FsRedirection :
	NonCopyable
{
public:
	ScopedDisableWow64FsRedirection();
	~ScopedDisableWow64FsRedirection();

private:
	bool	_redirected{};
	PVOID	_ctx{};

private:
	static bool IsWow64();
	static bool DisableWow64FsRedirection(PVOID* ctx);
	static bool RevertWow64FsRedirection(PVOID ctx);
};

void debug_print(char const* format, ...);

template <typename T>
void EnumWindows(T func)
{
	::EnumWindows(
		[](HWND hWnd, LPARAM ctx) -> BOOL
		{
			auto pfunc = reinterpret_cast<T*>(ctx);
			return (*pfunc)(hWnd) ? TRUE : FALSE;
		},
		reinterpret_cast<LPARAM>(&func)
	);
}

bool SuspendProcess(DWORD pid);
bool ResumeProcess(DWORD pid);

rectangle	GetWindowRect(HWND hWnd);
rectangle	GetClientRect(HWND hWnd);
rectangle	MapWindowPoints(HWND hWndFrom, HWND hWndTo, rectangle const& r);
point		MapWindowPoints(HWND hWndFrom, HWND hWndTo, point const& p);
point		GetCursorPos();
bool		SetCursorPos(point const& p);
int			CutoffRegion(scoped_gdi_region const& source, rectangle const& rect);
void		CenterWindow(HWND hWnd);