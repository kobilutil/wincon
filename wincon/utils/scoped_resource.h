#pragma once

template <typename T>
struct scoped_resource_traits;

template <typename T>
struct scoped_resource_traits_base
{
	using type = T;
	static bool is_empty(type v) { return !v; }
};

template <typename T, typename traits = scoped_resource_traits<T> >
class scoped_resource :
	NonCopyable
{
	using resource_type = typename traits::type;

public:
	explicit 
	scoped_resource(resource_type res = resource_type()) : _res(res)
	{}

	~scoped_resource()
	{
		reset();
	}

	scoped_resource(scoped_resource&& other) : scoped_resource()
	{
		*this = std::move(other);
	}

	scoped_resource& operator=(scoped_resource&& other)
	{
		if (this != &other)
		{
			reset();
			std::swap(_res, other._res);
		}
		return *this;
	}

	explicit operator bool() const		{ return !traits::is_empty(_res); }
	resource_type get() const			{ return _res; }

	resource_type detach()
	{
		auto v = _res;
		_res = resource_type();
		return v;
	}

	void reset(resource_type res = resource_type())
	{
		if (!traits::is_empty(_res))
			traits::release(_res);

		_res = res;
	}

private:
	resource_type	_res;
};
