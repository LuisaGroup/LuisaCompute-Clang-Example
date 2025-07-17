if not is_mode("debug") then
    target("clangcxx_compiler")
    _config_project({
        project_kind = "binary"
    })
    on_config(function(target)
        local _, ld = target:tool("ld")
        if ld == "link" then
            target:add("ldflags", "/STACK:8388608")
        elseif ld == "clang" or ld == "clangxx" then
            target:add("ldflags", "-Wl,-stack_size -Wl,8388608")
        elseif ld == "gcc" or ld == "gxx" then
            target:add("ldflags", "-Wl,--stack -Wl,8388608")
        end
    end)
    add_files("*.cpp")
    add_deps("lc-clangcxx", "lc-runtime", "lc-vstl", "lc-ast", "mimalloc", "lc-backends-dummy", "reproc")
    add_packages("yyjson", {
        public = false
    })
    target_end()
end