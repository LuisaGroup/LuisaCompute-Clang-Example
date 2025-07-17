#include <luisa/std.hpp>
using namespace luisa::shader;

float3 linear_to_srgb(float3 x) {
	return saturate(select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f,
						   12.92f * x,
						   x <= 0.00031308f));
};
using namespace luisa::shader;
[[kernel_2d(16, 8)]] int kernel(
	Image<float>& in_img,
	Image<float>& out_img,
	bool to_srgb) {
	uint2 coord = dispatch_id().xy;
	float3 result = in_img.load(coord).xyz;
	if (to_srgb) {
		result = linear_to_srgb(result);
	}
	out_img.store(coord, float4(result, 1));
	return 0;
}