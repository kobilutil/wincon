#include "stdafx.h"
#include "TrackingTooltip.h"
#include <CommCtrl.h>

bool TrackingTooltip::Create(HWND hWndOwner)
{
	HWND hWndTooltip = ::CreateWindowEx(0,
		TOOLTIPS_CLASS, 0, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hWndOwner, NULL, NULL, NULL);

	if (!hWndTooltip)
		return false;

	::SetWindowPos(hWndTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	TOOLINFO ti{};
	ti.cbSize = sizeof(TOOLINFO);
	ti.uFlags = TTF_ABSOLUTE | TTF_IDISHWND | TTF_TRACK;
	ti.hwnd = hWndOwner;
	ti.uId = (UINT)hWndOwner;

	if (!::SendMessage(hWndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti))
		return false;

	_hWndTooltip = hWndTooltip;
	_hWndOwner = hWndOwner;
	return true;
}

void TrackingTooltip::Show(bool doShow)
{
	TOOLINFO ti{};
	ti.cbSize = sizeof(TOOLINFO);
	ti.hwnd = _hWndOwner;
	ti.uId = (UINT)_hWndOwner;
	::SendMessage(_hWndTooltip, TTM_TRACKACTIVATE, (WPARAM)(doShow ? TRUE : FALSE), (LPARAM)&ti);
}

void TrackingTooltip::SetText(LPWSTR str)
{
	TOOLINFO ti{};
	ti.cbSize = sizeof(TOOLINFO);
	ti.hwnd = _hWndOwner;
	ti.uId = (UINT)_hWndOwner;
	ti.lpszText = str;
	::SendMessageW(_hWndTooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
}

void TrackingTooltip::SetPosition(point const& pt)
{
	::SendMessage(_hWndTooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x(), pt.y()));
}
