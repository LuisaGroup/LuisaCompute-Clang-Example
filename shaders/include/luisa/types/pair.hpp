#pragma once
#include "../attributes.hpp"
#include "../type_traits.hpp"

namespace luisa::shader {
// constexpr inline struct zero_then_variadic_t {
// } zero_then_variadic;
// constexpr inline struct one_then_variadic_t {
// } one_then_variadic;

template<class first_type, class second_type>
class pair {
public:
	first_type first;
	second_type second;

	constexpr pair() noexcept(is_constructible_v<first_type> && is_constructible_v<second_type>)
		: first(), second() {}

	// template<class... Ts2>
	// constexpr explicit pair(
	// 	zero_then_variadic_t,
	// 	Ts2&&... val2) noexcept(is_constructible_v<first_type> && is_constructible_v<second_type, Ts2...>)
	// 	: first(), second(static_cast<Ts2&&>(val2)...) {}

	// template<class T1, class... Ts2>
	// constexpr pair(
	// 	one_then_variadic_t,
	// 	T1&& val1,
	// 	Ts2&&... val2) noexcept(is_constructible_v<first_type, T1> && is_constructible_v<second_type, Ts2...>)
	// 	: first(static_cast<T1&&>(val1)), second(static_cast<Ts2&&>(val2)...) {}

	template<class T1, class T2>
	constexpr pair(
		T1&& val1,
		T2&& val2) noexcept(is_constructible_v<first_type, T1> && is_constructible_v<second_type, T2>)
		: first(static_cast<T1&&>(val1)), second(static_cast<T2&&>(val2)) {}

	template<uint64 I>
	auto get() const -> conditional_t<I == 1, second_type, first_type> {
		if constexpr (I == 1) {
			return second;
		} else {
			return first;
		}
	}
};

template<class first_type, class second_type>
constexpr pair<remove_cvref_t<first_type>, remove_cvref_t<second_type>> make_pair(first_type&& first, second_type&& second) {
	return {static_cast<first_type&&>(first), static_cast<second_type&&>(second)};
}
}// namespace luisa::shader

namespace std {
template<class E>
class tuple_size;

template<size_t I, class E>
class tuple_element;

template<class T1, class T2>
trait_struct tuple_size<luisa::shader::pair<T1, T2>> : integral_constant<uint64, 2>{};

template<class T1, class T2>
trait_struct tuple_element<0, luisa::shader::pair<T1, T2>> {
	using type = T1;
};
template<class T1, class T2>
trait_struct tuple_element<1, luisa::shader::pair<T1, T2>> {
	using type = T2;
};
}// namespace std
