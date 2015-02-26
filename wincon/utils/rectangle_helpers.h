#pragma once

#include "rectangle.h"

using size = basic_size < long >;
using point = basic_point < long >;
using rectangle = basic_rectangle < long >;

template <typename T>
void operator<<(basic_size<T>& v1, SIZE const& v2)
{
	v1.width() = v2.cx;
	v1.height() = v2.cy;
}

template <typename T>
void operator<<(SIZE& v1, basic_size<T> const& v2)
{
	v1.cx = v2.width();
	v1.cy = v2.height();
}

template <typename T>
void operator<<(basic_size<T>& v1, COORD const& v2)
{
	v1.width() = v2.X;
	v1.height() = v2.Y;
}

template <typename T>
void operator<<(COORD& v1, basic_size<T> const& v2)
{
	// TODO: (search for "warning C4244" down below for more details)
	v1.X = (SHORT)v2.width();
	v1.Y = (SHORT)v2.height();
}

template <typename T>
void operator<<(basic_point<T>& v1, POINT const& v2)
{
	v1.x() = v2.x;
	v1.y() = v2.y;
}

template <typename T>
void operator<<(POINT& v1, basic_point<T> const& v2)
{
	v1.x = v2.x();
	v1.y = v2.y();
}

template <typename T>
void operator<<(basic_point<T>& v1, COORD const& v2)
{
	v1.x() = v2.X;
	v1.y() = v2.Y;
}

template <typename T>
void operator<<(COORD& v1, basic_point<T> const& v2)
{
	v1.X = (SHORT)v2.x();
	v1.Y = (SHORT)v2.y();
}

template <typename T>
void operator<<(basic_rectangle<T>& v1, RECT const& v2)
{
	v1.left() = v2.left;
	v1.top() = v2.top;
	v1.width() = v2.right - v2.left;
	v1.height() = v2.bottom - v2.top;
}

template <typename T>
void operator<<(RECT& v1, basic_rectangle<T> const& v2)
{
	v1.left = v2.left();
	v1.top = v2.top();
	v1.right = v2.right();
	v1.bottom = v2.bottom();
}

template <typename T>
void operator<<(basic_rectangle<T>& v1, SMALL_RECT const& v2)
{
	v1.left() = v2.Left;
	v1.top() = v2.Top;
	v1.width() = v2.Right - v2.Left + 1;
	v1.height() = v2.Bottom - v2.Top + 1;
}

template <typename T>
void operator<<(SMALL_RECT& v1, basic_rectangle<T> const& v2)
{
	// the casting to SHORT removes the following warning
	// warning C4244: '=' : conversion from 'const long' to 'SHORT', possible loss of data
	// TODO: find a better solution.
	// maybe use some sort of numeric_cast<> that checks in runtime that no data was actually lost
	v1.Left = (SHORT)v2.left();
	v1.Top = (SHORT)v2.top();
	v1.Right = (SHORT)v2.right() - 1;
	v1.Bottom = (SHORT)v2.bottom() - 1;
}
