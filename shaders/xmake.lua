target("_build_shader")
set_kind("phony")
add_deps("clangcxx_compiler")
after_build(function(target)
    local builddir = path.absolute(path.directory(target:targetdir()), os.projectdir())
    local shader_out_dir
    local compiler_path = "clangcxx_compiler"
    compiler_path = path.join(target:targetdir(), "clangcxx_compiler")
    if os.is_host("windows") then
        compiler_path = compiler_path .. ".exe"
    end
    local shader_dir = path.translate(path.join(os.projectdir(), "shaders"))
    local backends = {"dx", "vk", "metal"}
    for i, backend in ipairs(backends) do
        if os.exists(path.join(target:targetdir(), "lc-backend-" .. backend .. ".dll")) then
            print("compiling " .. backend .. " shader")
            shader_out_dir = path.translate(path.join(builddir, "shader_build_" .. backend))
            os.execv(compiler_path,
                {"--in=" .. path.translate(path.join(shader_dir, "src")), "--out=" .. shader_out_dir, "--backend=" .. backend,
                 "--cache_dir=" .. builddir, "--include=" .. path.translate(path.join(shader_dir, "include"))}) 
        end
    end
end)
target_end()
