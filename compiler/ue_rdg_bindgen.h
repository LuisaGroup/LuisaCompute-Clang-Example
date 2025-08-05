#pragma once

#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include "string_builder.h"
struct UERDGBindGen {
    using MapType = vstd::HashMap<vstd::string, vstd::function<void(vstd::StringBuilder &)>>;
    static void codegen(
        vstd::StringBuilder& sb,
        MapType const &replace_funcs,
        luisa::string_view template_type,
    char replace_char);
};