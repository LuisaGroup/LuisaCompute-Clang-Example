#include "common/cornell_box.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "common/tiny_obj_loader.h"
#include <luisa/core/logging.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/bindless_array.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/gui/window.h>
using namespace luisa;
using namespace luisa::compute;
int main(int argc, char *argv[]) {

    Context context{argv[0]};
    if (argc <= 1) {
        LUISA_INFO("Usage: {} <backend>. <backend>: cuda, dx, cpu, metal", argv[0]);
        exit(1);
    }
    // load the Cornell Box scene
    tinyobj::ObjReaderConfig obj_reader_config;
    obj_reader_config.triangulate = true;
    obj_reader_config.vertex_color = false;
    tinyobj::ObjReader obj_reader;
    if (!obj_reader.ParseFromString(obj_string, "", obj_reader_config)) {
        luisa::string_view error_message = "unknown error.";
        if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
        LUISA_ERROR_WITH_LOCATION("Failed to load OBJ file: {}", error_message);
    }
    if (auto &&e = obj_reader.Warning(); !e.empty()) {
        LUISA_WARNING_WITH_LOCATION("{}", e);
    }
    auto &&p = obj_reader.GetAttrib().vertices;
    luisa::vector<float3> vertices;
    vertices.reserve(p.size() / 3u);
    for (uint i = 0u; i < p.size(); i += 3u) {
        vertices.emplace_back(make_float3(
            p[i + 0u], p[i + 1u], p[i + 2u]));
    }
    LUISA_INFO(
        "Loaded mesh with {} shape(s) and {} vertices.",
        obj_reader.GetShapes().size(), vertices.size());

    Device device = context.create_device(argv[1]);
    BindlessArray heap = device.create_bindless_array(65535, BindlessSlotType::BUFFER_ONLY);
    Stream stream = device.create_stream(StreamTag::GRAPHICS);
    Buffer<float3> vertex_buffer = device.create_buffer<float3>(vertices.size());
    stream << vertex_buffer.copy_from(vertices.data());
    luisa::vector<Mesh> meshes;
    luisa::vector<Buffer<Triangle>> triangle_buffers;
    for (auto &&shape : obj_reader.GetShapes()) {
        uint index = static_cast<uint>(meshes.size());
        std::vector<tinyobj::index_t> const &t = shape.mesh.indices;
        uint triangle_count = t.size() / 3u;
        LUISA_INFO("Processing shape '{}' at index {} with {} triangle(s).",
                   shape.name, index, triangle_count);
        luisa::vector<uint> indices;
        indices.reserve(t.size());
        for (tinyobj::index_t i : t) {
            indices.emplace_back(i.vertex_index);
        }
        Buffer<Triangle> &triangle_buffer = triangle_buffers.emplace_back(
            device.create_buffer<Triangle>(triangle_count));
        Mesh &mesh =
            meshes.emplace_back(device.create_mesh(vertex_buffer, triangle_buffer));
        heap.emplace_on_update(index, triangle_buffer);
        stream << triangle_buffer.copy_from(indices.data()) << mesh.build();
    }

    Accel accel = device.create_accel({});
    for (Mesh &m : meshes) {
        accel.emplace_back(m, make_float4x4(1.0f));
    }
    stream << heap.update()
           << accel.build();
    luisa::filesystem::path shader_path{luisa::string{"../shader_build_"} + argv[1]};
    auto tracing_shader = device.load_shader<2,
                                             BindlessArray,
                                             Accel,
                                             Buffer<float3>,// materials,
                                             Image<uint>,   // seed_image,
                                             Buffer<float3>,// vertex_buffer,
                                             Image<float>,  // image,
                                             uint,          //spp_per_dispatch,
                                             uint2,         //resolution,
                                             uint,          //mesh_size,
                                             uint           //frame_index
                                             >(luisa::to_string(shader_path / "test_path_tracing.bin"));
    auto present_shader = device.load_shader<2, Image<float>, Image<float>, bool>(luisa::to_string(shader_path / "present.bin"));
    const uint2 resolution(1024u);
    Image<uint> seed_image = device.create_image<uint>(PixelStorage::INT1, resolution);
    Image<float> accum_image = device.create_image<float>(PixelStorage::FLOAT4, resolution);

    Window window{"path tracing", resolution};
    Swapchain swap_chain = device.create_swapchain(
        stream,
        SwapchainOption{
            .display = window.native_display(),
            .window = window.native_handle(),
            .size = make_uint2(resolution),
            .wants_hdr = false,
            .wants_vsync = false,
            .back_buffer_count = 8,
        });

    Image<float> ldr_image = device.create_image<float>(swap_chain.backend_storage(), resolution);
    float3 mats[] = {
        make_float3(0.725f, 0.710f, 0.680f),// floor
        make_float3(0.725f, 0.710f, 0.680f),// ceiling
        make_float3(0.725f, 0.710f, 0.680f),// back wall
        make_float3(0.140f, 0.450f, 0.091f),// right wall
        make_float3(0.630f, 0.065f, 0.050f),// left wall
        make_float3(0.725f, 0.710f, 0.680f),// short box
        make_float3(0.725f, 0.710f, 0.680f),// tall box
        make_float3(0.000f, 0.000f, 0.000f) // light
    };
    auto material_buffer = device.create_buffer<float3>(8);
    stream << material_buffer.copy_from(mats);
    uint frame_count = 0u;
    while (!window.should_close()) {
        CommandList cmdlist;
        cmdlist << tracing_shader(heap, accel, material_buffer, seed_image, vertex_buffer, accum_image, 16, resolution, 8, frame_count).dispatch(resolution)
                << present_shader(accum_image, ldr_image, luisa::string_view{argv[1]} != "vk").dispatch(resolution);
        cmdlist.add_present(swap_chain.present(ldr_image));
        stream << cmdlist.commit();
        frame_count += 1;
        window.poll_events();
    }
}