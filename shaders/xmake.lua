target("_build_shader")
set_kind("phony")
add_deps("clangcxx_compiler")
after_build(function(target)
    local builddir = path.absolute(path.directory(target:targetdir()), os.projectdir())
    local shader_out_dir = path.translate(path.join(builddir, "shader_build_dx"))
    local compiler_path = "clangcxx_compiler"
    compiler_path = path.join(target:targetdir(), "clangcxx_compiler")
    if os.is_host("windows") then
        compiler_path = compiler_path .. ".exe"
    end
    local shader_dir = path.translate(path.join(os.projectdir(), "shaders"))
    os.execv(
        compiler_path,
        {"--in=" .. path.translate(path.join(shader_dir, "src")),
        "--out=" .. shader_out_dir,
        "--backend=dx",
        "--cache_dir=" .. builddir,
        "--include="..path.translate(path.join(shader_dir, "include"))}
    )
    shader_out_dir = path.translate(path.join(builddir, "shader_build_vk"))
    os.execv(
        compiler_path,
        {"--in=" .. path.translate(path.join(shader_dir, "src")),
        "--out=" .. shader_out_dir,
        "--backend=vk",
        "--cache_dir=" .. builddir,
        "--include="..path.translate(path.join(shader_dir, "include"))}
    )
end)
target_end()