#include <luisa/backends/ext/dx_config_ext.h>
#include <iostream>
#include <luisa/core/clock.h>
#include <luisa/core/logging.h>
#include <luisa/core/binary_file_stream.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/lmdb.hpp>
#include <luisa/vstl/md5.h>
#include <luisa/clangcxx/compiler.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/context.h>
#include <luisa/core/binary_file_stream.h>
#include <luisa/vstl/v_guid.h>
#include <luisa/vstl/spin_mutex.h>
#include <mimalloc.h>
#include <luisa/vstl/lockfree_array_queue.h>
#ifdef _WIN32
#include <windows.h>
#endif
#undef ERROR
#undef Yield
#undef byte
#include "preprocessor.h"
#include <reproc++/reproc.hpp>
#include "ue_rdg_bindgen.h"
using namespace luisa;
using namespace luisa::compute;

luisa::unique_ptr<DeviceConfigExt> create_dxc_config();
static bool kTestRuntime = false;
string to_lower(string_view value) {
    string value_str{value};
    for (auto &i : value_str) {
        if (i >= 'A' && i <= 'Z') {
            i += 'a' - 'A';
        }
    }
    return value_str;
}
static void _dummy_function_for_get_binary_path() {}
luisa::filesystem::path get_binary_path() {
    luisa::filesystem::path result;
#ifdef _WIN32
    TCHAR dllPath[MAX_PATH];
    {
        HMODULE hm = NULL;
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&_dummy_function_for_get_binary_path, &hm);
        GetModuleFileName(hm, dllPath, MAX_PATH);
    }
    result = luisa::filesystem::path(dllPath).parent_path();
    SetDllDirectory(result.c_str());
#else
    static_assert(false, "root-path not supported in current platform.");
#endif
    return result;
}
class DefineIter : public vstd::IRange<luisa::string_view> {
    luisa::unordered_set<luisa::string> const *first_map;
    luisa::span<luisa::string_view> const *second_map;
    luisa::unordered_set<luisa::string>::const_iterator first_iter;
    luisa::span<luisa::string_view>::const_iterator second_iter;
    luisa::string_view value;

public:
    vstd::IteRef<IRange> begin() noexcept override {
        first_iter = first_map->begin();
        second_iter = second_map->begin();
        if (first_iter != first_map->end()) {
            value = *first_iter;
        } else if (second_iter != second_map->end()) {
            value = *second_iter;
        }
        return vstd::IteRef<IRange>{this};
    }
    bool operator==(vstd::IteEndTag) const noexcept {
        return first_iter == first_map->end() && second_iter == second_map->end();
    }
    void operator++() noexcept override {
        if (first_iter == first_map->end()) {
            value = *second_iter;
            ++second_iter;
        } else {
            value = *first_iter;
            ++first_iter;
        }
    }
    luisa::string_view operator*() noexcept override {
        return value;
    }
    DefineIter(
        luisa::unordered_set<luisa::string> const &first_map,
        luisa::span<luisa::string_view> const &second_map)
        : first_map(&first_map),
          second_map(&second_map) {
    }
};
int compile(
    luisa::filesystem::path src_path,
    luisa::filesystem::path dst_path,
    Device &device,
    luisa::unordered_set<luisa::string> const &defines,
    luisa::span<luisa::string_view> extra_defines,
    luisa::vector<std::filesystem::path> const &inc_paths,
    bool use_optimize,
    bool enable_debug_info,
    bool ue,
    luisa::string virtual_path,
    luisa::filesystem::path project_path,
    luisa::filesystem::path ue_host_path) {

    if (dst_path.empty()) {
        dst_path = src_path.filename();
        if (ue) {
            if (dst_path.has_extension() && dst_path.extension() == "usf") {
                dst_path.replace_extension();
                dst_path.replace_extension(luisa::to_string(dst_path.filename()) + string("_out.usf"sv));
            } else {
                dst_path.replace_extension("usf");
            }
        } else {
            if (dst_path.has_extension() && dst_path.extension() == "bin") {
                dst_path.replace_extension();
                dst_path.replace_extension(luisa::to_string(dst_path.filename()) + string("_out.bin"sv));
            } else {
                dst_path.replace_extension("bin");
            }
        }
    }
    auto curr_path = luisa::filesystem::current_path();
    auto bin_path = get_binary_path();
    if (ue) {
        if (!project_path.is_absolute()) {
            project_path = (curr_path / project_path).lexically_normal();
        }
        if (!ue_host_path.is_absolute()) {
            ue_host_path = (project_path / ue_host_path).lexically_normal();
        }
        if (!dst_path.is_absolute()) {
            dst_path = (project_path / dst_path).lexically_normal();
        }

        if (!luisa::filesystem::exists(ue_host_path) && !ue_host_path.empty()) {
            luisa::filesystem::create_directories(ue_host_path);
        }
    }
    // format_path();
    DefineIter define_iter(defines, extra_defines);
    auto inc_iter = vstd::range_linker{
        vstd::make_ite_range(inc_paths),
        vstd::transform_range{
            [&](auto &&path) { return luisa::to_string(path); }}}
                        .i_range();
    luisa::clangcxx::ShaderReflection refl;
    if (!luisa::clangcxx::Compiler::create_shader(
            ShaderOption{
                .enable_fast_math = use_optimize,
                .enable_debug_info = !use_optimize && enable_debug_info,
                .compile_only = true,
                .name = luisa::to_string(dst_path)},
            device, define_iter, src_path, inc_iter, ue ? &refl : nullptr)) {
        return 1;
    }
    if (ue) {
        auto shader_name = luisa::to_string(dst_path.replace_extension("").filename());
        auto relative_dst_path = luisa::filesystem::relative(dst_path, project_path);
        // LUISA_WARNING("dst_path {}", luisa::to_string(dst_path));
        // LUISA_WARNING("ue_host_path {}", luisa::to_string(ue_host_path));
        // LUISA_WARNING("shader_name {}", shader_name);
        UERDGBindGen::gen_compute_bind(virtual_path, shader_name, bin_path / "gen_template.h", bin_path / "gen_template.cpp", refl, ue_host_path);
    }
    return 0;
}
class BinaryFileStreamInterface : public BinaryStream {

private:
    BinaryFileStream _stream;
public:
    explicit BinaryFileStreamInterface(const luisa::string &path) noexcept
        : _stream{path} {}
    ~BinaryFileStreamInterface() noexcept override {
    }
    [[nodiscard]] size_t length() const noexcept override { return _stream.length(); }
    [[nodiscard]] size_t pos() const noexcept override { return _stream.pos(); }
    void read(luisa::span<std::byte> dst) noexcept override {
        _stream.read(dst);
    }
};

class WriteUSF : public BinaryIO {
public:
    luisa::compute::Context ctx;
    WriteUSF(luisa::compute::Context &ctx) : ctx(ctx) {}
    luisa::unique_ptr<BinaryStream> read_shader_bytecode(luisa::string_view name) const noexcept override {
        std::filesystem::path local_path{name};
        if (local_path.is_absolute()) {
            return luisa::make_unique<BinaryFileStreamInterface>(luisa::string(name));
        }
        auto file_path = luisa::to_string(ctx.runtime_directory() / name);
        return luisa::make_unique<BinaryFileStreamInterface>(file_path);
    };
    luisa::unique_ptr<BinaryStream> read_shader_cache(luisa::string_view name) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    };
    luisa::unique_ptr<BinaryStream> read_internal_shader(luisa::string_view name) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    };
    luisa::filesystem::path write_shader_bytecode(luisa::string_view name, luisa::span<std::byte const> data) const noexcept override {
        luisa::string str;
        std::filesystem::path local_path{name};
        if (local_path.is_absolute()) {
            str = name;
        } else {
            str = luisa::to_string(ctx.runtime_directory() / name);
        }
        luisa::filesystem::path str_path{str};
        if (str_path.has_parent_path()) {
            luisa::filesystem::create_directories(str_path.parent_path());
        }
        auto include_path = "#include \"/Engine/Public/Platform.ush\"\n"sv;
        {
            BinaryFileStream file_stream(str);
            if (file_stream.valid() && file_stream.length() == include_path.size() + data.size()) {
                luisa::vector<std::byte> disk_data;
                disk_data.push_back_uninitialized(file_stream.length());
                file_stream.read(disk_data);
                if (std::memcmp(include_path.data(), disk_data.data(), include_path.size()) == 0 &&
                    std::memcmp(data.data(), disk_data.data() + include_path.size(), data.size()) == 0) {
                    return name;
                }
            }
        }

        auto f = fopen(str.c_str(), "wb");
        if (f) {
            fwrite(include_path.data(), include_path.size(), 1, f);
            fwrite(data.data(), data.size(), 1, f);
            fclose(f);
        } else {
            LUISA_ERROR("Write {} failed.", name);
        }
        return str;
    };
    luisa::filesystem::path write_shader_cache(luisa::string_view name, luisa::span<std::byte const> data) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    };
    luisa::filesystem::path write_internal_shader(luisa::string_view name, luisa::span<std::byte const> data) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    };
    luisa::unique_ptr<BinaryStream> read_shader_source(luisa::string_view name) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    };
    luisa::filesystem::path write_shader_source(luisa::string_view name, luisa::span<std::byte const> data) const noexcept override {
        LUISA_NOT_IMPLEMENTED();
        return {};
    }
    void clear_shader_cache() const noexcept override {}
};
int main(int argc, char *argv[]) {
    log_level_warning();
    //////// Properties
    if (argc <= 1) {
        LUISA_ERROR("Empty argument not allowed.");
    }
    std::filesystem::path src_path;
    std::filesystem::path dst_path;
    luisa::vector<std::filesystem::path> inc_paths;
    luisa::unordered_set<luisa::string> defines;
    luisa::string backend = "dx";
    Context context{argv[0]};
    bool use_optimize = true;
    bool enable_debug_info = false;
    luisa::filesystem::path ue_virtual_path;
    luisa::filesystem::path project_path;
    luisa::filesystem::path ue_host_path;
    {
        bool enable_help = false;
        bool enable_lsp = false;
        bool rebuild = false;
        bool pack_lmdb = false;
        std::filesystem::path pack_path;
        vstd::HashMap<vstd::string, vstd::function<void(vstd::string_view)>> cmds(16);
        auto invalid_arg = []() {
            LUISA_ERROR("Invalid argument, use --help please.");
        };

        cmds.emplace(
            "opt"sv,
            [&](string_view name) {
                auto lower_name = to_lower(name);
                if (lower_name == "on"sv) {
                    use_optimize = true;
                } else if (lower_name == "off"sv) {
                    use_optimize = false;
                } else {
                    invalid_arg();
                }
            });
        cmds.emplace(
            "help"sv,
            [&](string_view name) {
                enable_help = true;
            });
        cmds.emplace(
            "project_path"sv,
            [&](string_view name) {
                project_path = name;
            });
        cmds.emplace(
            "ue_host_path"sv,
            [&](string_view name) {
                ue_host_path = name;
            });
        cmds.emplace(
            "ue_virtual_path"sv,
            [&](string_view name) {
                ue_virtual_path = name;
            });
        cmds.emplace(
            "backend"sv,
            [&](string_view name) {
                if (name.empty()) {
                    invalid_arg();
                }
                auto lower_name = to_lower(name);
                backend = lower_name;
            });
        cmds.emplace(
            "in"sv,
            [&](string_view name) {
                if (name.empty()) {
                    invalid_arg();
                }
                if (!src_path.empty()) {
                    LUISA_ERROR("Source path set multiple times.");
                }
                src_path = name;
            });
        cmds.emplace(
            "out"sv,
            [&](string_view name) {
                if (name.empty()) {
                    invalid_arg();
                }
                if (!dst_path.empty()) {
                    LUISA_ERROR("Dest path set multiple times.");
                }
                dst_path = name;
            });
        cmds.emplace(
            "include"sv,
            [&](string_view name) {
                if (name.empty()) {
                    invalid_arg();
                }
                inc_paths.emplace_back(name);
            });

        cmds.emplace(
            "D"sv,
            [&](string_view name) {
                if (name.empty()) {
                    invalid_arg();
                }
                defines.emplace(name);
            });
        cmds.emplace(
            "lsp"sv,
            [&](string_view name) {
                enable_lsp = true;
            });
        cmds.emplace(
            "rebuild"sv,
            [&](string_view name) {
                rebuild = true;
            });
        cmds.emplace(
            "pack_path"sv,
            [&](string_view name) {
                pack_path = name;
                pack_lmdb = true;
            });
        cmds.emplace(
            "debug"sv,
            [&](string_view) {
                enable_debug_info = true;
            });
        auto cache_path = luisa::filesystem::current_path() / ".cache";
        cmds.emplace(
            "cache_dir"sv,
            [&](string_view sv) {
                cache_path = luisa::filesystem::path{sv} / ".cache";
            });

        // TODO: define
        for (auto i : vstd::ptr_range(argv + 1, argc - 1)) {
            string arg = i;
            string_view kv_pair = arg;
            for (auto i : vstd::range(arg.size())) {
                if (arg[i] == '-')
                    continue;
                else {
                    kv_pair = string_view(arg.data() + i, arg.size() - i);
                    break;
                }
            }
            if (kv_pair.empty() || kv_pair.size() == arg.size()) {
                invalid_arg();
            }
            string_view key = kv_pair;
            string_view value;
            for (auto i : vstd::range(kv_pair.size())) {
                if (kv_pair[i] == '=') {
                    key = string_view(kv_pair.data(), i);
                    value = string_view(kv_pair.data() + i + 1, kv_pair.size() - i - 1);
                    break;
                }
            }
            auto iter = cmds.find(key);
            if (!iter) {
                invalid_arg();
            }
            iter.value()(value);
        }
        if (enable_help) {
            // print help list
            string_view helplist = R"(
Argument format:
    argument should be -ARG=VALUE or --ARG=VALUE, invalid argument will cause fatal error and never been ignored.

Argument list:
    --opt: enable or disable optimize, E.g --opt=on, --opt=off, case ignored.
    --backend: backend name, currently support "dx", "cuda", "metal", case ignored.
    --in: input file or dir, E.g --in=./my_dir/my_shader.cpp
    --out: output file or dir, E.g --out=./my_dir/my_shader.bin
    --include: include file directory, E.g --include=./shader_dir/
    --D: shader predefines, this can be set multiple times, E.g --D=MY_MACRO
    --lsp: enable compile_commands.json generation, E.g --lsp
    --ue_host_path: UE global shader *.h & *.cpp path
    --project_path: shader's source project path for relative path
)"sv;
            std::cout << helplist << '\n';
            return 0;
        }
        if (src_path.empty()) {
            LUISA_ERROR("Input file path not defined.");
        }
        cache_path = cache_path.lexically_normal();
        if (rebuild) {

            if (std::filesystem::exists(cache_path)) {
                std::error_code ec;
                std::filesystem::remove_all(cache_path, ec);
                if (ec) [[unlikely]] {
                    LUISA_ERROR("Try clear cache dir {} failed {}.", luisa::to_string(cache_path), ec.message());
                }
            }
            if (std::filesystem::exists(dst_path)) {
                std::error_code ec;
                std::filesystem::remove_all(dst_path, ec);
                if (ec) [[unlikely]] {
                    LUISA_ERROR("Try clear out dir {} failed {}.", luisa::to_string(dst_path), ec.message());
                }
            }
        }
        if (src_path.is_relative()) {
            src_path = std::filesystem::current_path() / src_path;
        }
        std::error_code code;
        src_path = std::filesystem::canonical(src_path, code);
        if (code.value() != 0) {
            LUISA_ERROR("Invalid source file path {} {}", luisa::to_string(src_path), code.message());
        }
        auto format_path = [&]() {
            for (auto &&i : inc_paths) {
                if (i.is_relative()) {
                    i = std::filesystem::current_path() / i;
                }
                i = std::filesystem::weakly_canonical(i, code);
                if (code.value() != 0) {
                    LUISA_ERROR("Invalid include file path");
                }
            }

            if (dst_path.is_relative()) {
                dst_path = std::filesystem::current_path() / dst_path;
            }
            if (src_path == dst_path) {
                LUISA_ERROR("Source file and dest file path can not be the same.");
            }
            dst_path = std::filesystem::weakly_canonical(dst_path, code);
            if (code.value() != 0) {
                LUISA_ERROR("Invalid dest file path");
            }
        };
        auto ite_dir = [&](auto &&ite_dir, auto const &path, auto &&func) -> void {
            for (auto &i : std::filesystem::directory_iterator(path)) {
                if (i.is_directory()) {
                    auto path_str = luisa::to_string(i.path().filename());
                    if (path_str[0] == '.') {
                        continue;
                    }
                    ite_dir(ite_dir, i.path(), func);
                    continue;
                }
                func(i.path());
            }
        };
        //////// LSP print
        if (enable_lsp) {
            // BindLog bind_log;
            if (!std::filesystem::is_directory(src_path)) {
                LUISA_ERROR("Source path must be a directory.");
            }
            if (dst_path.empty()) {
                dst_path = "compile_commands.json";
            } else if (std::filesystem::exists(dst_path) && !std::filesystem::is_regular_file(dst_path)) {
                LUISA_ERROR("Dest path must be a file.");
            }
            if (inc_paths.empty()) {
                inc_paths.emplace_back(src_path);
            }
            format_path();

            luisa::vector<char> result;
            result.reserve(16384);
            result.emplace_back('[');
            luisa::vector<std::filesystem::path> paths;
            auto func = [&](auto const &file_path_ref) {
                // auto file_path = file_path_ref;
                auto const &ext = file_path_ref.extension();
                if (ext != ".cpp" && ext != ".h" && ext != ".hpp") return;
                paths.emplace_back(file_path_ref);
            };
            ite_dir(ite_dir, src_path, func);
            if (!paths.empty()) {
                luisa::fiber::scheduler thread_pool(std::min<uint>(std::thread::hardware_concurrency(), paths.size()));
                std::mutex mtx;
                luisa::fiber::parallel(paths.size(), [&](size_t i) {
                    auto &file_path = paths[i];
                    if (file_path.is_absolute()) {
                        file_path = std::filesystem::relative(file_path, src_path);
                    }
                    luisa::vector<char> local_result;
                    auto iter = vstd::range_linker{
                        vstd::make_ite_range(defines),
                        vstd::transform_range{[&](auto &&v) { return luisa::string_view{v}; }}}
                                    .i_range();
                    auto inc_iter = vstd::range_linker{
                        vstd::make_ite_range(inc_paths),
                        vstd::transform_range{
                            [&](auto &&path) { return luisa::to_string(path); }}}
                                        .i_range();
                    luisa::clangcxx::Compiler::lsp_compile_commands(
                        iter,
                        src_path,
                        file_path,
                        inc_iter,
                        local_result);
                    local_result.emplace_back(',');
                    size_t idx = [&]() {
                        std::lock_guard lck{mtx};
                        auto sz = result.size();
                        result.push_back_uninitialized(local_result.size());
                        return sz;
                    }();
                    memcpy(result.data() + idx, local_result.data(), local_result.size());
                });
            }
            if (result.size() > 1) {
                result.pop_back();
            }
            result.emplace_back(']');
            auto dst_path_str = luisa::to_string(dst_path);
            auto f = fopen(dst_path_str.c_str(), "wb");
            fwrite(result.data(), result.size(), 1, f);
            fclose(f);
            return 0;
        }
        //////// Compile all
        if (std::filesystem::is_directory(src_path)) {
            // BindLog bind_log;
            luisa::vector<std::pair<luisa::string, int32_t>> failed_paths;
            luisa::spin_mutex failed_paths_mtx;
            if (dst_path.empty()) {
                dst_path = src_path / "out";
            } else if (std::filesystem::exists(dst_path) && !std::filesystem::is_directory(dst_path)) {
                LUISA_ERROR("Dest path must be a directory.");
            }
            luisa::vector<std::filesystem::path> paths;
            auto create_dir = [&](auto &&path) {
                auto const &parent_path = path.parent_path();
                if (!std::filesystem::exists(parent_path))
                    std::filesystem::create_directories(parent_path);
            };
            auto func = [&](auto const &file_path_ref) {
                auto const &ext = file_path_ref.extension();
                if (ext == ".cpp") {
                    paths.emplace_back(file_path_ref);
                }
            };
            ite_dir(ite_dir, src_path, func);
            if (paths.empty()) return 0;
            luisa::fiber::scheduler thread_pool(std::min<uint>(std::thread::hardware_concurrency(), paths.size()));

            format_path();
            log_level_info();
            auto iter = vstd::range_linker{
                vstd::make_ite_range(defines),
                vstd::transform_range{[&](auto &&v) { return luisa::string_view{v}; }}}
                            .i_range();
            auto inc_iter = vstd::range_linker{
                vstd::make_ite_range(inc_paths),
                vstd::transform_range{
                    [&](auto &&path) { return luisa::to_string(path); }}}
                                .i_range();

            Preprocessor processor{
                cache_path / ".lmdb",
                cache_path / ".obj",
                iter,
                inc_iter};
            std::atomic_bool success = true;

            luisa::fiber::parallel(
                paths.size(),
                [&](size_t idx) {
                    auto const &src_file_path = paths[idx];
                    auto file_path = src_file_path;
                    if (file_path.is_absolute()) {
                        file_path = std::filesystem::relative(file_path, src_path);
                    }
                    auto out_path = dst_path / file_path;
                    if (!ue_virtual_path.empty())
                        out_path.replace_extension("usf");
                    else
                        out_path.replace_extension("bin");
                    if (!processor.require_recompile(src_path, file_path) && luisa::filesystem::exists(out_path)) return;
                    create_dir(out_path);
                    int result = 0;
                    auto exec_func = [&](luisa::span<luisa::string_view> extra_defines, int time) {
                        luisa::vector<char const *> vec;
                        luisa::vector<luisa::string> vec_datas;
                        auto local_out_path = out_path;
                        auto out_filename = luisa::to_string(local_out_path.replace_extension("").filename());
                        for (auto &i : extra_defines) {
                            if (i.empty()) continue;
                            out_filename += "_";
                            out_filename += i;
                        }
                        local_out_path.replace_filename(out_filename);
                        if (!ue_virtual_path.empty())
                            local_out_path.replace_extension(".usf");
                        else
                            local_out_path.replace_extension(".bin");
                        vec_datas.emplace_back(argv[0]);
                        vec_datas.emplace_back(luisa::format("-project_path={}", luisa::to_string(project_path)));
                        if (!ue_host_path.empty()) {
                            vec_datas.emplace_back(luisa::format("-ue_host_path={}", luisa::to_string(ue_host_path / file_path.parent_path())));
                        }
                        vec_datas.emplace_back(luisa::format("-opt={}", use_optimize ? "on"sv : "off"sv));
                        if (enable_debug_info) {
                            vec_datas.emplace_back("-debug"sv);
                        }
                        if (!ue_virtual_path.empty()) {
                            vec_datas.emplace_back(luisa::format("-ue_virtual_path={}", luisa::to_string(ue_virtual_path / file_path.parent_path())));
                        }
                        vec_datas.emplace_back(luisa::format("-backend={}", backend));
                        vec_datas.emplace_back(luisa::format("-in={}", luisa::to_string(src_file_path)));
                        vec_datas.emplace_back(luisa::format("-out={}", luisa::to_string(local_out_path)));
                        for (auto &i : inc_paths) {
                            vec_datas.emplace_back(luisa::format("-include={}", luisa::to_string(i)));
                        }
                        for (auto &i : defines) {
                            vec_datas.emplace_back(luisa::format("-D={}", i));
                        }
                        luisa::string macro;
                        for (auto &i : extra_defines) {
                            if (i.empty()) continue;
                            vec_datas.emplace_back(luisa::format("-D={}", i));
                            macro += " ";
                            macro += i;
                        }
                        vec.reserve(vec_datas.size() + 1);
                        vstd::push_back_func(vec, vec_datas.size(), [&](size_t i) {
                            return vec_datas[i].c_str();
                        });
                        vec.push_back(nullptr);
                        if (time > 0) {
                            LUISA_WARNING("compiling {} internal error, retrying time {}...", luisa::to_string(file_path.filename()), time);
                        } else
                            LUISA_INFO("compiling {}{}", luisa::to_string(file_path.filename()), macro);
                        reproc::options o;
                        // setup the options
                        o.redirect.in.type = reproc::redirect::default_;
                        o.redirect.err.type = reproc::redirect::default_;
                        o.redirect.out.type = reproc::redirect::default_;

                        reproc::process p;
                        if (auto error = p.start(reproc::arguments{vec.data()}, o)) {
                            LUISA_WARNING("Process start error: {}", error.message());
                            result = 1;
                        } else {
                            auto wait_result = p.wait(1024h /* almost forever */);
                            if (wait_result.first || wait_result.second) {
                                result = wait_result.first;
                            }
                        }
                    };
                    // No variant
                    for (int i = 0; i < 5; ++i) {
                        result = 0;
                        exec_func({}, i);
                        if (result == 0 || result == 1) break;
                    }
                    if (result != 0) {
                        processor.remove_file(luisa::to_string(std::filesystem::weakly_canonical(src_path / file_path)));
                        success = false;
                        failed_paths_mtx.lock();
                        failed_paths.emplace_back(luisa::to_string(std::filesystem::weakly_canonical(src_path / file_path)), result);
                        failed_paths_mtx.unlock();
                    }
                });
            processor.post_process();
            if (pack_lmdb) {
                auto tmp_pack_dir = dst_path / ".pack_shader_tmp";
                {
                    vstd::vector<std::filesystem::directory_entry> dir_entries;
                    for (auto &&i : std::filesystem::recursive_directory_iterator(dst_path)) {
                        dir_entries.emplace_back(std::move(i));
                    }
                    vstd::LMDB tmp_pack_db{tmp_pack_dir};
                    luisa::vector<std::byte> bytes;
                    std::error_code ec;
                    for (auto &i : dir_entries) {
                        bool is_regular = i.is_regular_file(ec);
                        if (ec) [[unlikely]] {
                            LUISA_ERROR("Pack internal error.");
                        }
                        if (!is_regular) {
                            continue;
                        }
                        std::filesystem::path f = i;
                        if (f.is_absolute()) {
                            f = std::filesystem::relative(f, dst_path, ec);
                            if (ec) [[unlikely]] {
                                LUISA_ERROR("Pack internal error.");
                            }
                        }
                        auto path_str = luisa::to_string(f);
#ifdef _WIN32
                        for (auto &i : path_str) {
                            if (i == '\\') i = '/';
                        }
#endif
                        {
                            bytes.clear();
                            BinaryFileStream fs{luisa::to_string(i)};
                            bytes.resize_uninitialized(fs.length());
                            fs.read(bytes);
                        }
                        tmp_pack_db.write(path_str, bytes);
                        // TODO
                    }
                    auto internal_data_dir = context.runtime_directory() / ".data";
                    if (std::filesystem::is_directory(internal_data_dir) && !std::filesystem::is_empty(internal_data_dir)) {
                        {
                            vstd::LMDB internal_lmdb{internal_data_dir};
                            for (auto &i : internal_lmdb) {
                                luisa::string str{reinterpret_cast<char const *>(i.key.data()), i.key.size_bytes() / sizeof(char)};
                                str += "#"sv;
                                tmp_pack_db.write(str, i.value);
                            }
                        }
                    }
                    tmp_pack_db.copy_to(pack_path);
                }
                std::error_code ec;
                std::filesystem::remove_all(tmp_pack_dir, ec);
                if (ec) [[unlikely]] {
                    LUISA_ERROR(
                        "Failed to remove dir '{}': {}.",
                        luisa::to_string(tmp_pack_dir), ec.message());
                }
            }
            if (!success) {
                for (auto &i : failed_paths) {
                    LUISA_WARNING("Failed {} with code {}", i.first, i.second);
                }
            }
            return success ? 0 : -1;
        }
    }
    // luisa::set_custom_logger(clangcxx_log);
    //////// Compile
    struct MyConfig : public DirectXDeviceConfigExt {
        bool LoadDXC() const noexcept { return false; }
    };
    DeviceConfig config{
        .extension = luisa::make_unique<MyConfig>(),
        .headless = true};
    BinaryIO *bin_io = nullptr;
    if (!ue_virtual_path.empty()) {
        bin_io = new WriteUSF(context);
        config.binary_io = bin_io;
    }
    auto device = context.create_device(backend, &config);
    return compile(
        std::move(src_path),
        std::move(dst_path),
        device,
        defines,
        {},
        inc_paths,
        use_optimize,
        enable_debug_info,
        !ue_virtual_path.empty(),
        luisa::to_string(ue_virtual_path),
        std::move(project_path),
        std::move(ue_host_path));
}
