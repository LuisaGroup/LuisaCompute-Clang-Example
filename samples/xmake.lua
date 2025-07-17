target("test_path_tracing")
_config_project({
    project_kind = "binary"
})
add_files("test_path_tracing.cpp")
add_deps("lc-runtime", "lc-backends-dummy", "lc-gui")
target_end()
