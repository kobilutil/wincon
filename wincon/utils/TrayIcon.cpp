#include "stdafx.h"
#include "TrayIcon.h"


//void ConsolesMonitorApp::Init()
//{
//	::WNDCLASSEX wcex = { 0 };
//	wcex.cbSize = sizeof(wcex);
//	wcex.hInstance = ::GetModuleHandle(NULL);
//	wcex.lpszClassName = L"37FCC3A1-BC9D-4F41-A49C-0A185457A915";
//	wcex.lpfnWndProc = Static_WndProc;
//
//	auto wndClassAtom = ::RegisterClassEx(&wcex);
//	if (!wndClassAtom)
//		return;
//
//	::CreateWindowEx(0, wcex.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);
//}


void TrayIcon::Create(HWND hWnd, HICON hIcon)
{
	hWnd_ = hWnd;
	id_ = GetNextId();
	callbackMessage_ = GetCallbackMessage();

	auto data = FillDefaultStruct();

	data.uFlags = NIF_MESSAGE | NIF_ICON;
	data.uCallbackMessage = callbackMessage_;
	data.hIcon = hIcon;

	::Shell_NotifyIcon(NIM_ADD, &data);
}

void TrayIcon::Remove()
{
	auto data = FillDefaultStruct();
	::Shell_NotifyIcon(NIM_DELETE, &data);
}

void TrayIcon::ChangeTooltip(LPCWSTR tooltip)
{
	auto data = FillDefaultStruct();
	data.uFlags = NIF_TIP; // | NIF_SHOWTIP;
	::lstrcpy(data.szTip, tooltip);
	::Shell_NotifyIcon(NIM_MODIFY, &data);
}

void TrayIcon::ShowBalloon(LPCWSTR title, LPCWSTR info, DWORD flags/*, UINT timeout*/)
{
	auto data = FillDefaultStruct();
	data.uFlags = NIF_INFO;
	data.dwInfoFlags = flags;
	::lstrcpy(data.szInfoTitle, title);
	::lstrcpy(data.szInfo, info);
	//data.uTimeout = 15000;
	::Shell_NotifyIcon(NIM_MODIFY, &data);
}

bool TrayIcon::ProcessMessage(UINT msg, WPARAM wParam) const
{
	return (msg == callbackMessage_) && (wParam == id_);
}

UINT TrayIcon::GetNextId()
{
	static UINT id = 0;
	return ++id;
}

UINT TrayIcon::GetCallbackMessage()
{
	static UINT msg = ::RegisterWindowMessage(L"TrayIcon-callback-message-0D88BC8C-9D59-426A-A887-C6452044DC0C");
	return msg;
}

::NOTIFYICONDATA TrayIcon::FillDefaultStruct() const
{
	::NOTIFYICONDATA data = {};
	data.cbSize = sizeof(data);
	data.hWnd = hWnd_;
	data.uID = id_;
	return data;
}
