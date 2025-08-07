#pragma once

#include <luisa/vstl/common.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/binary_io.h>
#include <luisa/vstl/functional.h>
#include <luisa/clangcxx/build_arguments.h>
#include "string_builder.h"
struct UERDGBindGen {
    using MapType = vstd::HashMap<vstd::string, vstd::function<void(vstd::StringBuilder &)>>;
    static void codegen(
        vstd::StringBuilder &sb,
        MapType const &replace_funcs,
        luisa::string_view template_type,
        char replace_char);
    static void gen_compute_bind(
        luisa::string_view local_virtual_path,
        luisa::string_view shader_name,
        luisa::filesystem::path const &header_template_path,
        luisa::filesystem::path const &impl_template_path,
        luisa::clangcxx::ShaderReflection const &refl,
        luisa::filesystem::path const &host_output_path);
};