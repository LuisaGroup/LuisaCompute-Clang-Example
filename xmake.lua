set_policy("build.ccache", not is_plat("windows"))
set_policy("check.auto_ignore_flags", false)

if (is_os("windows")) then 
    add_defines("UNICODE", "_UNICODE", "NOMINMAX", "_WINDOWS")
    add_defines("_GAMING_DESKTOP")
    add_defines("_CRT_SECURE_NO_WARNINGS")
    add_defines("_ENABLE_EXTENDED_ALIGNED_STORAGE")
    add_defines("_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR") -- for preventing std::mutex crash when lock
    if (is_mode("release")) then
        set_runtimes("MD")
    elseif (is_mode("asan")) then
        add_defines("_DISABLE_VECTOR_ANNOTATION")
    -- else
    --     set_runtimes("MDd")
    end
end

lc_options = {
    cpu_backend = false,
    cuda_backend = true,
    dx_backend = is_host("windows"),
    vk_backend = is_host("windows"),
    enable_cuda = true,
    enable_api = false,
    enable_clangcxx = true,
    enable_dsl = true,
    enable_gui = true,
    enable_osl = false,
    enable_ir = false,
    enable_tests = false,
    lc_backend_lto = false,
    vk_support = true,
    metal_backend = false,
    dx_cuda_interop = false,
    lc_win_runtime = "MD",
    sdk_dir = "LuisaCompute/SDKs",
    -- toy_c_backend = true
}
includes("LuisaCompute")
includes("compiler")
-- includes("samples")
-- includes("shaders")