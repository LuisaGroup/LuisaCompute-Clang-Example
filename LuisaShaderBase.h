#pragma once
#include <type_traits>
#include <stdint.h>

namespace luisa
{
	template <typename T, size_t align>
		requires(alignof(T) <= align)
	struct AlignedType
	{
		alignas(align) T _data;
		operator T const&() const & { return _data; }
		operator T&() & { return _data; }
		operator T&&() && { return std::move(_data); }

		template <typename... Args>
			requires(std::is_constructible_v<T, Args&&...>)
		AlignedType(Args&&... args): _data(std::forward<Args>(args)...)
		{
		}

		template <typename Arg>
			requires(std::is_constructible_v<T, Arg>)
		T& operator=(Arg&& arg)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
				std::destroy_at(&_data);
			std::construct_at(&_data, std::forward<Arg>(arg));
			return _data;
		}
	};

	template <typename T, size_t align, size_t size>
	requires(sizeof(T) <= size)
	struct ArgType
	{
		alignas(align) char _data[size];

		operator T() const &
		{
			T t;
			std::memcpy(&t, _data, sizeof(T));
			return t;
		}

		template <typename... Args>
			requires(std::is_constructible_v<T, Args&&...>)
		ArgType(Args&&... args)
		{
			T t(std::forward<Args>(args)...);
			std::memcpy(&_data, &t, sizeof(T));
		}

		template <typename Arg>
			requires(std::is_constructible_v<T, Arg>)
		void operator=(Arg&& arg)
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
				std::destroy_at(&_data);
			T t(std::forward<Arg>(arg));
			std::memcpy(&_data, &t, sizeof(T));
		}
	};
}
