#pragma once

#include <shellapi.h>


class TrayIcon
{
public:
	void Create(HWND hWnd, HICON hIcon);

	void Remove();

	void ChangeTooltip(LPCWSTR tooltip);

	void ShowBalloon(LPCWSTR title, LPCWSTR info, DWORD flags/*, UINT timeout*/);

	bool ProcessMessage(UINT msg, WPARAM wParam) const;

private:
	static UINT GetNextId();

	static UINT GetCallbackMessage();

	::NOTIFYICONDATA FillDefaultStruct() const;

private:
	HWND	hWnd_;
	UINT	id_;
	UINT	callbackMessage_;
};
