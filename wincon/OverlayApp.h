#pragma once
#include "utils\utils.h"

class OverlayApp : NonCopyable
{
	// private CTOR/DTOR
	OverlayApp() = default;
	~OverlayApp() = default;

public:
	static int Run();
};
