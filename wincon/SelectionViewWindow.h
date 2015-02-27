#pragma once

#include "utils\utils.h"
#include "utils\ConsoleHelper.h"
#include "SelectionHelper.h"


class SelectionViewWindow
{
	using SimpleWndProc = SimpleWndProc < SelectionViewWindow > ;
	friend SimpleWndProc;

public:
	SelectionViewWindow(ConsoleHelper& consoleHelper, SelectionHelper& selectionHelper);
	~SelectionViewWindow();

	bool Create(HWND hWndOwner, HWND hWndOverlay);

	void AdjustPosition();
	void Refresh();

private:
	LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

	void OnWM_Paint(HWND hwnd);
	BOOL OnWM_EraseBackground(HWND hwnd, HDC hdc);

private:
	HWND			_hWnd;
	HWND			_hWndConsole;
	HWND			_hWndOverlay;

	ConsoleHelper&		_consoleHelper;
	SelectionHelper&	_selectionHelper;

	scoped_gdi_region	_selectionRegion;
};