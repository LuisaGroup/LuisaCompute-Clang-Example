#pragma once
#include "./../attributes.hpp"

using int16 = short;
using uint16 = unsigned short;
using int32 = int;
using int64 = long long;
using uint32 = unsigned int;
using uint = uint32;
using uint64 = unsigned long long;

template<typename F>
inline constexpr bool is_function_v = __is_function(F);

template<typename T, typename U>
inline constexpr bool is_same_v = __is_same_as(T, U);

template<class T, template<class...> class Z, class... Ts>
concept require = Z<T, Ts...>::value;

template<class T, class U>
concept same_as = is_same_v<T, U> && is_same_v<U, T>;

template<class Fn, class... Args>
concept invocable = requires(
	Fn&& fn, Args&&... args) { static_cast<Fn&&>(fn)(static_cast<Args&&>(args)...); };

template<typename T, typename Arg>
inline constexpr bool is_assignable_v = __is_assignable(T, Arg);

template<class T, class... Args>
constexpr bool is_constructible_v = __is_constructible(T, Args...);

template<typename T, template<typename...> typename Template>
inline constexpr bool is_specialization_v = false;// true if && only if T is a specialization of Template

template<template<typename...> typename Template, typename... Args>
inline constexpr bool is_specialization_v<Template<Args...>, Template> = true;

template<typename T, template<typename, uint32> typename Template>
inline constexpr bool is_specialization_resource_v = false;// true if && only if T is a specialization of Template

template<template<typename, uint32> typename Template, typename Arg>
inline constexpr bool is_specialization_resource_v<Template<Arg, 0u>, Template> = true;

template<typename T>
inline constexpr bool is_void_v = __is_void(T);

template<typename T>
using decay_t = __decay(T);

template<typename T>
static constexpr bool is_signed_v = __is_signed(T);

template<typename T>
using signed_t = __make_signed(T);

template<typename T>
static constexpr bool is_unsigned_v = __is_unsigned(T);

template<typename T>
using unsigned_t = __make_unsigned(T);

template<typename T>
using remove_extent_t = __remove_extent(T);

template<typename T>
static constexpr bool is_reference_v = __is_reference(T);

template<typename T>
using remove_reference_t = __remove_reference_t(T);

template<typename T>
static constexpr bool is_const_v = __is_const(T);

template<typename T>
using remove_const_t = __remove_const(T);

template<typename T>
static constexpr bool is_volatile_v = __is_volatile(T);

template<typename T>
using remove_volatile_t = __remove_volatile(T);

template<typename T>
using remove_cv_t = __remove_cv(T);

template<typename T>
using remove_cvref_t = __remove_cvref(T);

template<typename T, T v>
trait_struct integral_constant {
	static constexpr const T value = v;
	typedef T value_type;
	typedef integral_constant type;
	explicit constexpr operator value_type() const { return value; }
	constexpr value_type operator()() const { return value; }
};
typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template<bool B, class T, class F>
trait_struct conditional {
	using type = T;
};
template<class T, class F>
trait_struct conditional<false, T, F> {
	using type = F;
};
template<bool B, class T, class F>
using conditional_t = typename conditional<B, T, F>::type;

namespace luisa::shader {

struct [[builtin("half")]] half {
	[[ignore]] half() = default;
	[[ignore]] half(float);
	[[ignore]] half(uint32);
	[[ignore]] half(int32);
	[[cast]] operator float() const;
	[[cast]] operator uint() const;
	[[cast]] operator int() const;

private:
	short v;
};

}// namespace luisa::shader