#pragma once
#include "../attributes.hpp"
#include "../type_traits.hpp"
#include "../resources/buffer.hpp"

#include <std/type_traits>
#include <std/array>

namespace luisa::shader {

template<typename Type, uint32 size>
	requires(std::is_same_v<Type, float> || std::is_same_v<Type, half>)
struct [[builtin("coop_vec")]] CooperativeVector {
	static constexpr uint32 N = size;
	constexpr CooperativeVector() = default;

	template<uint32 start, typename... Args>
	[[noignore]] constexpr void set(Type v, Args... args) {
		set(start, v);
		set<start + 1>(args...);
	}
	template<uint32 start>
	[[noignore]] constexpr void set(Type v) { set(start, v); }
	[[noignore]] constexpr void set(uint32 loc, Type v) { access_(loc) = v; }
	[[access]] constexpr Type& access_(uint32 loc) { return v[loc]; }
	[[access]] constexpr Type& operator[](uint32 loc) { return v[loc]; }

	[[noignore]] constexpr Type get(uint32 loc) const { return access_(loc); }
	[[access]] constexpr Type access_(uint32 loc) const { return v[loc]; }
	[[access]] constexpr Type operator[](uint32 loc) const { return v[loc]; }

private:
	// DONT EDIT THIS FIELD LAYOUT
	Type v[size];
};
template<typename OutputType, typename InputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_ADD_FP8E5M2")]] CooperativeVector<OutputType, N> cooperative_mul_add_fp8e5m2(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	ByteBuffer<>& bias_buffer,
	uint32 bias_offset,
	CooperativeVector<InputType, M> layer);

template<typename OutputType, typename InputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_FP8E5M2")]] CooperativeVector<OutputType, N> cooperative_mul_fp8e5m2(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	CooperativeVector<InputType, M> layer);

template<typename OutputType, typename InputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_ADD_FP8E4M3")]] CooperativeVector<OutputType, N> cooperative_mul_add_fp8e4m3(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	ByteBuffer<>& bias_buffer,
	uint32 bias_offset,
	CooperativeVector<InputType, M> layer);

template<typename OutputType, typename InputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_FP8E4M3")]] CooperativeVector<OutputType, N> cooperative_mul_fp8e4m3(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	CooperativeVector<InputType, M> layer);

template<typename OutputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_ADD_FP16")]] CooperativeVector<OutputType, N> cooperative_mul_add(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	ByteBuffer<>& bias_buffer,
	uint32 bias_offset,
	CooperativeVector<half, M> layer);
template<typename OutputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_FP16")]] CooperativeVector<OutputType, N> cooperative_mul(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	CooperativeVector<half, M> layer);

template<typename OutputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_ADD_FP32")]] CooperativeVector<OutputType, N> cooperative_mul_add(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	ByteBuffer<>& bias_buffer,
	uint32 bias_offset,
	CooperativeVector<float, M> layer);

template<typename OutputType, uint32 N, uint32 M>
[[callop("COOPERATIVE_MUL_FP32")]] CooperativeVector<OutputType, N> cooperative_mul(
	ByteBuffer<>& matrix_buffer,
	uint32 matrix_offset,
	CooperativeVector<float, M> layer);
}// namespace luisa::shader