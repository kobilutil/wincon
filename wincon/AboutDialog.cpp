#include "stdafx.h"
#include "AboutDialog.h"
#include "Resource.h"


void AboutDialog::ShowModal(HWND ownerHwnd)
{
	static HWND s_hDlg = nullptr;

	if (!s_hDlg)
	{
		::DialogBox(::GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), ownerHwnd, 
			[](HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) -> INT_PTR
			{
				switch (msg)
				{
				case WM_INITDIALOG:
					s_hDlg = hDlg;
					return (INT_PTR)TRUE;
				case WM_NCDESTROY:
					s_hDlg = nullptr;
					break;
				case WM_COMMAND:
					if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
					{
						::EndDialog(hDlg, LOWORD(wParam));
						return (INT_PTR)TRUE;
					}
					break;
				}
				return (INT_PTR)FALSE;
			}
		);

		s_hDlg = nullptr;
	}
	else
	{
		::SetForegroundWindow(s_hDlg);
	}
}
