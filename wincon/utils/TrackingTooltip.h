#pragma once
#include "utils.h"

class TrackingTooltip :
	NonCopyable
{
public:
	bool Create(HWND hWndOwner);
	void Show(bool doShow);
	void SetText(LPWSTR str);
	void SetPosition(point const& pt);

private:
	HWND	_hWndTooltip;
	HWND	_hWndOwner;
};
