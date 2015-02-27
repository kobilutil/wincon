#pragma once

template <typename T, typename R = T>
struct enable_if_type { typedef R type; };

template <typename T>
class basic_size
{
public:
	using value_type = T;
	using self_type = basic_size < T >;

	basic_size() : basic_size(value_type(), value_type())
	{}

	basic_size(value_type w, value_type h) : _w(w), _h(h)
	{}

	template <typename U>
	basic_size(basic_size<U> const& other) : _w(other.w()), _h(other.h())
	{}

	template <typename U>
	basic_size& operator=(basic_size<U> const& other)
	{
		_w = other.w();
		_h = other.h();
		return *this;
	}

	value_type& width()		{ return _w; }
	value_type& height()	{ return _h; }

	value_type const& width() const		{ return _w; }
	value_type const& height() const	{ return _h; }

	template <typename U>
	basic_size& operator+=(basic_size<U> const& other)
	{
		_w += other.width();
		_h += other.height();
		return *this;
	}

	template <typename U>
	basic_size& operator-=(basic_size<U> const& other)
	{
		_w -= other.width();
		_h -= other.height();
		return *this;
	}

	template <typename U>
	basic_size& operator/=(basic_size<U> const& other)
	{
		_w /= other.width();
		_h /= other.height();
		return *this;
	}

	template <typename U>
	basic_size& operator*=(basic_size<U> const& other)
	{
		_w *= other.width();
		_h *= other.height();
		return *this;
	}

private:
	value_type	_w;
	value_type	_h;
};

template <typename T>
class basic_point
{
public:
	using value_type = T;
	using self_type = basic_point < T >;

	basic_point() : basic_point(value_type(), value_type())
	{}

	basic_point(value_type x, value_type y) : _x(x), _y(y)
	{}

	template <typename U>
	basic_point(basic_point<U> const& other) : _x(other.x()), _y(other.y())
	{}

	template <typename U>
	basic_point& operator=(basic_point<U> const& other)
	{
		_x = other.x();
		_y = other.y();
		return *this;
	}

	value_type& x()		{ return _x; }
	value_type& y()		{ return _y; }

	value_type const& x() const		{ return _x; }
	value_type const& y() const		{ return _y; }

#define DEFINE_OPERATOR(OP, TYPE, P1, P2) \
	template <typename U>								\
	basic_point& operator OP=(TYPE<U> const& other)		\
	{\
		_x OP= other.P1();		\
		_y OP= other.P2();		\
		return *this;			\
	}

	DEFINE_OPERATOR(+ , basic_point, x, y);
	DEFINE_OPERATOR(- , basic_point, x, y);
	DEFINE_OPERATOR(/ , basic_point, x, y);
	DEFINE_OPERATOR(* , basic_point, x, y);

	DEFINE_OPERATOR(+ , basic_size, width, height);
	DEFINE_OPERATOR(- , basic_size, width, height);
	DEFINE_OPERATOR(/ , basic_size, width, height);
	DEFINE_OPERATOR(* , basic_size, width, height);

#undef DEFINE_OPERATOR

private:
	value_type	_x;
	value_type	_y;
};

template <typename T>
class basic_rectangle
{
public:
	using value_type = T;
	using self_type = basic_rectangle < T >;
	using point_type = basic_point < T >;
	using size_type = basic_size < T >;

	basic_rectangle()
	{}

	basic_rectangle(value_type x, value_type y, value_type w, value_type h) : _point(x, y), _size(w, h)
	{}

	template <typename U, typename V>
	basic_rectangle(basic_point<U> point, basic_size<V> size) : _point(point), _size(size)
	{}

	template <typename U>
	basic_rectangle(basic_rectangle<U> const& other) : _point(other.top_left()), _size(other.size())
	{}

	template <typename U>
	basic_rectangle& operator=(basic_rectangle<U> const& other)
	{
		_point = other.top_left();
		_size = other.size();
		return *this;
	}

	value_type& left()			{ return _point.x(); }
	value_type& top()			{ return _point.y(); }
	value_type& width()			{ return _size.width(); }
	value_type& height()		{ return _size.height(); }

	value_type const& left() const		{ return _point.x(); }
	value_type const& top() const		{ return _point.y(); }
	value_type const& width() const		{ return _size.width(); }
	value_type const& height() const	{ return _size.height(); }

	point_type&	top_left()	{ return _point; }
	size_type&	size()		{ return _size; }

	point_type const& top_left() const	{ return _point; }
	size_type const& size() const		{ return _size; }

	value_type right() const	{ return _point.x() + _size.width(); }
	value_type bottom() const	{ return _point.y() + _size.height(); }

	point_type const bottom_right() const	{ return _point + _size; }

	template <typename U>
	bool contains(basic_point<U> const& p) const
	{
		// TODO: explain why the following doesn't work (because p2 is exclusive)
		//return ((top_left() <= p) && (p < bottom_right()));

		if (p < top_left())
			return false;

		auto bottomRight = bottom_right();

		// note: this is not the same as (p >= bottomRight)
		if ((p.x() >= bottomRight.x()) || (p.y() >= bottomRight.y()))
			return false;

		return true;
	}

	template <typename U>
	basic_rectangle& operator+=(basic_point<U> const& other)
	{
		_point += other;
		return *this;
	}

	template <typename U>
	basic_rectangle& operator+=(basic_size<U> const& other)
	{
		_size += other;
		return *this;
	}

	template <typename U>
	basic_rectangle& operator-=(basic_point<U> const& other)
	{
		_point -= other;
		return *this;
	}

	template <typename U>
	basic_rectangle& operator-=(basic_size<U> const& other)
	{
		_size -= other;
		return *this;
	}

private:
	point_type	_point;
	size_type	_size;
};

template <typename T1, typename T2>
typename T1::self_type const operator+(T1 const& v1, T2 const& v2)
{
	auto result = v1;
	result += v2;
	return result;
}

template <typename T1, typename T2>
typename T1::self_type const operator-(T1 const& v1, T2 const& v2)
{
	auto result = v1;
	result -= v2;
	return result;
}

template <typename T1, typename T2>
typename T1::self_type const operator*(T1 const& v1, T2 const& v2)
{
	auto result = v1;
	result *= v2;
	return result;
}

template <typename T1, typename T2>
typename T1::self_type const operator/(T1 const& v1, T2 const& v2)
{
	auto result = v1;
	result /= v2;
	return result;
}


template <typename T1, typename T2>
inline typename enable_if_type<typename T1::self_type, bool>::type operator!=(T1 const& v1, T2 const& v2)	{ return !(v1 == v2); }

template <typename T1, typename T2>
inline bool operator==(basic_size<T1> const& v1, basic_size<T2> const& v2)
{
	return (v1.width() == v2.width()) && (v1.height() == v2.height());
}

template <typename T1, typename T2>
inline bool operator==(basic_point<T1> const& v1, basic_point<T2> const& v2)
{
	return (v1.x() == v2.x()) && (v1.y() == v2.y());
}

template <typename T1, typename T2>
inline bool operator==(basic_rectangle<T1> const& v1, basic_rectangle<T2> const& v2)
{
	return (v1.top_left() == v2.top_left()) && (v1.size() == v2.size());
}

template <typename T1, typename T2>
inline bool operator <(basic_point<T1> const& v1, basic_point<T2> const& v2)
{ 
	return (v1.y() < v2.y()) || (v1.y() == v2.y() && v1.x() < v2.x());
}

template <typename T1, typename T2>
inline bool operator <=(basic_point<T1> const& v1, basic_point<T2> const& v2)
{
	return (v1.y() < v2.y()) || (v1.y() == v2.y() && v1.x() <= v2.x());
}

template <typename T1, typename T2>
inline bool operator >(basic_point<T1> const& v1, basic_point<T2> const& v2)
{
	return (v1.y() > v2.y()) || (v1.y() == v2.y() && v1.x() > v2.x());
}

template <typename T1, typename T2>
inline bool operator >=(basic_point<T1> const& v1, basic_point<T2> const& v2)
{
	return (v1.y() > v2.y()) || (v1.y() == v2.y() && v1.x() >= v2.x());
}
