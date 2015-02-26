#pragma once

#include "scoped_resource.h"
#include <WTypes.h>


template<>
struct scoped_resource_traits<HANDLE> : scoped_resource_traits_base<HANDLE>
{
	static bool is_empty(type v)	{ return v == NULL || v == INVALID_HANDLE_VALUE; }
	static void release(type v)		{ ::CloseHandle(v); }
};

template<>
struct scoped_resource_traits<HWINEVENTHOOK> : scoped_resource_traits_base<HWINEVENTHOOK>
{
	static void release(type v)	{ ::UnhookWinEvent(v); }
};

template<>
struct scoped_resource_traits<HMENU> : scoped_resource_traits_base<HMENU>
{
	static void release(type v)	{ ::DestroyMenu(v); }
};

template <typename T>
struct scoped_resource_gdiobject_base
{
	using type = T;
	static bool is_empty(HGDIOBJ v) { return !v; }
	static void release(HGDIOBJ v)	{ ::DeleteObject(v); }
};

template<>
struct scoped_resource_traits<HRGN> : scoped_resource_gdiobject_base<HRGN>
{};

using scoped_handle = scoped_resource < HANDLE >;
using scoped_wineventhook = scoped_resource < HWINEVENTHOOK >;
using scoped_menu = scoped_resource < HMENU >;
using scoped_gdi_region = scoped_resource < HRGN >;
