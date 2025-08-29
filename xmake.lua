add_rules("mode.release", "mode.debug", "mode.releasedbg")
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
    lc_cpu_backend = false,
    lc_cuda_backend = true,
    lc_dx_backend = is_host("windows"),
    lc_vk_backend = is_host("windows"),
    lc_enable_cuda = true,
    lc_enable_api = false,
    lc_enable_clangcxx = true,
    lc_enable_dsl = true,
    lc_enable_gui = true,
    lc_enable_osl = false,
    lc_enable_ir = false,
    lc_enable_tests = false,
    lc_backend_lto = false,
    lc_vk_support = true,
    lc_metal_backend = false,
    lc_dx_cuda_interop = false,
    lc_win_runtime = "MD",
    lc_sdk_dir = "LuisaCompute/SDKs",
    lc_dx_sdk_dir = "D:/DXAgilitySDK"
    -- toy_c_backend = true
}
includes("LuisaCompute")
includes("compiler")
includes("samples")
includes("shaders")