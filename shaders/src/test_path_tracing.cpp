#include <luisa/std.hpp>
using namespace luisa::shader;
struct Onb {
	float3 tangent;
	float3 binormal;
	float3 normal;
	Onb(float3 n) {
		normal = n;
		binormal = normalize(ite(
			abs(normal.x) > abs(normal.z),
			float3(-normal.y, normal.x, 0.0f),
			float3(0.0f, -normal.z, normal.y)));
		tangent = normalize(cross(binormal, normal));
	}
	float3 to_world(float3 v) const {
		return v.x * tangent + v.y * binormal + v.z * normal;
	}
};
struct Triangle {
	uint i0;
	uint i1;
	uint i2;
};
float radians(float deg) { return deg * pi / 180.0f; }
float lcg(uint& state) {
	const uint lca = 1664525u;
	const uint lcc = 1013904223u;
	state = lca * state + lcc;
	return (float)(state & 0x00ffffffu) *
		   (1.0f / static_cast<float>(0x01000000u));
}
uint tea(uint v0, uint v1) {
	uint s0 = 0;
	for (uint n = 0u; n < 4u; n++) {
		s0 += 0x9e3779b9u;
		v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
		v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
	}
	return v0;
}
Ray generate_ray(float2 p) {
	const float fov = radians(27.8f);
	const float3 origin = float3(-0.01f, 0.995f, 5.0f);
	float3 pixel = origin + float3(p * tan(0.5f * fov), -1.0f);
	float3 direction = normalize(pixel - origin);
	return Ray(origin, direction);
};
template<typename T>
T balanced_heuristic(T pdf_a, T pdf_b) {
	return pdf_a / max(pdf_a + pdf_b, 1e-4f);
}
float3 offset_ray_origin(float3 p, float3 n) {
	const auto origin = 1.0f / 32.0f;
	const auto float_scale = 1.0f / 65536.0f;
	const auto int_scale = 256.0f;
	auto of_i = int3(int_scale * n);
	auto p_i = bit_cast<float3>(bit_cast<int3>(p) + ite(p < float3(0.0f), -of_i, of_i));
	return ite(abs(p) < origin, p + float_scale * n, p_i);
}
float3 cosine_sample_hemisphere(float2 u) {
	float r = sqrt(u.x);
	float phi = 2.0f * pi * u.y;
	return float3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
}

[[kernel_2d(16, 8)]] int kernel(
	BindlessBuffer& buffer_heap,
	Accel& accel,
	Buffer<float3>& materials,
	Image<uint>& seed_image,
	Buffer<float3>& vertex_buffer,
	Image<float>& image,
	uint spp_per_dispatch,
	uint2 resolution,
	uint mesh_size,
	uint frame_index) {
	uint2 coord = dispatch_id().xy;
	float frame_size = (float)min(resolution.x, resolution.y);
	uint state;
	if (frame_index > 0)
		state = seed_image.load(coord).x;
	else
		state = tea(coord.x, coord.y);
	float rx = lcg(state);
	float ry = lcg(state);
	float2 pixel = (float2(coord) + float2(rx, ry)) / frame_size * 2.0f - 1.0f;
	float3 radiance = (float3(0.0f));
	for (int i = 0; i < spp_per_dispatch; ++i) {
		Ray ray = generate_ray(pixel * float2(1.0f, -1.0f));
		float3 beta = (float3(1.0f));
		float pdf_bsdf = (0.0f);
		const float3 light_position = float3(-0.24f, 1.98f, 0.16f);
		const float3 light_u = float3(-0.24f, 1.98f, -0.22f) - light_position;
		const float3 light_v = float3(0.23f, 1.98f, 0.16f) - light_position;
		const float3 light_emission = float3(17.0f, 12.0f, 4.0f);
		float light_area = length(cross(light_u, light_v));
		float3 light_normal = normalize(cross(light_u, light_v));
		for (int depth = 0; depth < 10; ++depth) {
			// trace
			TriangleHit hit = accel.trace_closest(ray);
			if (hit.miss()) { break; }
			Triangle triangle = buffer_heap.buffer_read<Triangle>(hit.inst, hit.prim);
			float3 p0 = vertex_buffer.load(triangle.i0);
			float3 p1 = vertex_buffer.load(triangle.i1);
			float3 p2 = vertex_buffer.load(triangle.i2);
			float3 p = hit.interpolate(p0, p1, p2);
			float3 n = normalize(cross(p1 - p0, p2 - p0));
			float cos_wo = dot(-ray.dir(), n);
			if (cos_wo < 1e-4f) { break; }

			// hit light
			if (hit.inst == static_cast<uint>(mesh_size - 1u)) {
				if (depth == 0u) {
					radiance += light_emission;
				} else {
					float pdf_light = length_squared(p - ray.origin()) / (light_area * cos_wo);
					float mis_weight = balanced_heuristic(pdf_bsdf, pdf_light);
					radiance += mis_weight * beta * light_emission;
				}
				break;
			}

			// sample light
			float ux_light = lcg(state);
			float uy_light = lcg(state);
			float3 p_light = light_position + ux_light * light_u + uy_light * light_v;
			float3 pp = offset_ray_origin(p, n);
			float3 pp_light = offset_ray_origin(p_light, light_normal);
			float d_light = distance(pp, pp_light);
			float3 wi_light = normalize(pp_light - pp);
			Ray shadow_ray = Ray(offset_ray_origin(pp, n), wi_light, 0.f, d_light);
			bool occluded = accel.trace_any(shadow_ray);
			float cos_wi_light = dot(wi_light, n);
			float cos_light = -dot(light_normal, wi_light);
			float3 albedo = materials.load(hit.inst);
			if (!occluded && cos_wi_light > 1e-4f && cos_light > 1e-4f) {
				float pdf_light = (d_light * d_light) / (light_area * cos_light);
				float pdf_bsdf = cos_wi_light * inv_pi;
				float mis_weight = balanced_heuristic(pdf_light, pdf_bsdf);
				float3 bsdf = albedo * inv_pi * cos_wi_light;
				radiance += beta * bsdf * mis_weight * light_emission / max(pdf_light, 1e-4f);
			}

			// sample BSDF
			Onb onb(n);
			float ux = lcg(state);
			float uy = lcg(state);
			float3 wi_local = cosine_sample_hemisphere(float2(ux, uy));
			float cos_wi = abs(wi_local.z);
			float3 new_direction = onb.to_world(wi_local);
			ray = Ray(pp, new_direction);
			pdf_bsdf = cos_wi * inv_pi;
			beta *= albedo;// * cos_wi * inv_pi / pdf_bsdf => * 1.f

			// rr
			float l = dot(float3(0.212671f, 0.715160f, 0.072169f), beta);
			if (l == 0.0f) { break; }
			float q = max(l, 0.05f);
			float r = lcg(state);
			if (r >= q) { break; }
			beta *= 1.0f / q;
		}
	}
	radiance /= static_cast<float>(spp_per_dispatch);
	seed_image.store(coord, uint4(state));
	if (any(is_nan(radiance))) { radiance = float3(0.0f); }
	if (frame_index > 0) {
		auto old = image.load(coord).xyz;
		radiance = lerp(old, radiance, 1.f / (float)(frame_index + 1));
	}
	image.store(coord, float4(radiance, 1.0f));
	return 0;
}