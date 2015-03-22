#pragma once

class OverlayApp
{
	// private CTOR/DTOR
	OverlayApp() = default;
	~OverlayApp() = default;

public:
	static int Run(HWND hWndConsole);
};
