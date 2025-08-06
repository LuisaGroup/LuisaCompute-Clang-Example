#include "ue_rdg_bindgen.h"
#include <luisa/core/logging.h>
#include <luisa/core/binary_file_stream.h>
using namespace luisa;
using namespace luisa::compute;
void UERDGBindGen::codegen(
    vstd::StringBuilder &sb,
    MapType const &replace_funcs,
    luisa::string_view template_type,
    char replace_char) {
    auto end_ptr = template_type.data() + template_type.size();
    char const *ptr = template_type.data();
    char const *last_cached_ptr = ptr;
    auto clear_cache = [&](char const *ptr) {
        if (last_cached_ptr < ptr) {
            sb.append(luisa::string_view{last_cached_ptr, ptr});
            last_cached_ptr = ptr;
        }
    };
    while (ptr < end_ptr) {
        if (*ptr == replace_char) {
            clear_cache(ptr);
            ++ptr;
            auto key_begin = ptr;
            while (ptr < end_ptr) {
                if ((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= '0' && *ptr <= '9') || *ptr == '_') {
                    ++ptr;
                    continue;
                }
                break;
            }
            last_cached_ptr = ptr;
            auto iter = replace_funcs.find(luisa::string_view{key_begin, ptr});
            if (!iter) [[unlikely]] {
                LUISA_ERROR("Bad key {}", luisa::string_view{key_begin, ptr});
            }
            iter.value()(sb);
        } else {
            ++ptr;
        }
    }
    clear_cache(ptr);
}

void UERDGBindGen::gen_compute_bind(
    luisa::string_view local_virtual_path,
    luisa::string_view shader_name,
    luisa::filesystem::path const &header_template_path,
    luisa::filesystem::path const &impl_template_path,
    luisa::clangcxx::ShaderReflection const &refl,
    luisa::filesystem::path const &base_name) {
    MapType func_map;
    auto class_name = "F" + luisa::to_string(base_name.filename());
    if (class_name[1] >= 'a' && class_name[1] <= 'z') {
        class_name[1] -= 'a' - 'A';
    }
    auto class_dispatcher_name = class_name + "Dispatcher";
    func_map.emplace("ClsName", [&](vstd::StringBuilder &sb) {
        sb.append(class_name);
    });
    func_map.emplace("ClsDspName", [&](vstd::StringBuilder &sb) {
        sb.append(class_dispatcher_name);
    });
    size_t arg_type_size = 0;
    auto calc_align = [](uint c, uint a) -> uint {
        return (c + a - 1u) & (~(a - 1u));
    };
    luisa::vector<Type const *> struct_type_vec;
    luisa::unordered_map<Type const *, size_t> struct_type;
    {
        auto accum_struct = [&](auto &accum_struct, Type const *type) {
            if (!type->is_structure()) return;
            auto sz = struct_type.size();
            struct_type.try_emplace(type, sz);
            struct_type_vec.emplace_back(type);
            for (auto &i : type->members()) {
                accum_struct(accum_struct, i);
            }
        };
        for (auto &i : refl.kernel_args) {
            accum_struct(accum_struct, i.type);
        }
    }
    auto get_var_name = [&](vstd::StringBuilder &sb, Type const *type, uint id) {
        switch (type->tag()) {
            case Type::Tag::BUFFER:
                sb << luisa::format("_b{}", id);
                break;
            case Type::Tag::TEXTURE:
                sb << luisa::format("_t{}", id);
                break;
            case Type::Tag::BINDLESS_ARRAY:
                sb << luisa::format("_ba{}", id);
                break;
            case Type::Tag::ACCEL:
                sb << luisa::format("_ac{}", id);
                break;
            default:
                sb << luisa::format("v{}", id);
                break;
        }
    };
    auto get_type_name = [&](auto &&get_type_name, vstd::StringBuilder &sb, Type const *type, Usage usage) -> void {
        switch (type->tag()) {
            case Type::Tag::BOOL:
                sb.append("bool");
                break;
            case Type::Tag::INT16:
            case Type::Tag::FLOAT16:
            case Type::Tag::UINT16:
            case Type::Tag::UINT8:
            case Type::Tag::INT8:
                LUISA_ERROR("Unsupported type.");
                break;
            case Type::Tag::INT32:
                sb.append("int32_t");
                break;
            case Type::Tag::UINT32:
                sb.append("uint32_t");
                break;
            case Type::Tag::INT64:
                sb.append("int64_t");
                break;
            case Type::Tag::UINT64:
                sb.append("uint64_t");
                break;
            case Type::Tag::FLOAT32:
                sb.append("float");
                break;
            case Type::Tag::FLOAT64:
                sb.append("double");
                break;
            case Type::Tag::VECTOR:
                switch (type->element()->tag()) {
                    case Type::Tag::FLOAT32:
                        sb.append(luisa::format("FVector{}f", type->dimension()));
                        break;
                    case Type::Tag::INT32:
                        sb.append(luisa::format("FIntVector{}", type->dimension()));
                        break;
                    case Type::Tag::UINT32:
                        sb.append(luisa::format("FUintVector{}", type->dimension()));
                        break;
                    default:
                        LUISA_ERROR("Unsupported vector type {}", type->description());
                }
                break;
            case Type::Tag::MATRIX:
                switch (type->dimension()) {
                    case 2:
                        sb.append("FMatrix2x2");
                        break;
                    case 3:
                        sb.append("FMatrix3x4");
                        break;
                    case 4:
                        sb.append("FMatrix4x4");
                        break;
                    default:
                        LUISA_ERROR("Unsupported matrix type {}", type->description());
                }
                break;
            case Type::Tag::ARRAY: {
                sb << "std::array<";
                bool req_align = !type->element()->is_scalar();
                if (req_align) {
                    sb << "::luisa::AlignedType<";
                }
                get_type_name(get_type_name, sb, type->element(), usage);
                if (req_align) {
                    sb << luisa::format(", {}>", type->element()->alignment());
                }
                sb << ", " << luisa::format("{}", type->dimension()) << '>';
                
            } break;
            case Type::Tag::STRUCTURE: {
                auto iter = struct_type.find(type);
                if (iter == struct_type.end()) [[unlikely]] {
                    LUISA_ERROR("Internal error.");
                }
                sb << 'S' << iter->second;
            } break;
            case Type::Tag::BUFFER:
                if ((luisa::to_underlying(usage) & luisa::to_underlying(Usage::WRITE)) > 0)
                    sb << "FRDGBufferUAVRef";
                else
                    sb << "FRDGBufferSRVRef";
                break;
            case Type::Tag::TEXTURE:
                if ((luisa::to_underlying(usage) & luisa::to_underlying(Usage::WRITE)) > 0)
                    sb << "FRDGTextureUAVRef";
                else
                    sb << "FRDGTextureSRVRef";
                break;
            default:
                LUISA_ERROR("Invalid type.");
                break;
        }
    };
    vstd::StringBuilder sb;
    for (auto next_ptr = struct_type_vec.data() + struct_type_vec.size(); next_ptr != struct_type_vec.data(); --next_ptr) {
        auto &v = next_ptr[-1];
        sb << "struct ";
        get_type_name(get_type_name, sb, v, Usage::NONE);
    }
    func_map.emplace("ArgType", [&](vstd::StringBuilder &sb) {
        sb.append("\tstruct Args {\n");
        for (auto &i : refl.kernel_args) {
            if (!i.type->is_resource()) {
                sb << "\t\t";
            }
            switch (i.type->tag()) {
                case Type::Tag::BOOL:
                    arg_type_size += 4;
                    sb.append(luisa::format("alignas(4) bool {};\n", i.var_name));
                    break;
                case Type::Tag::INT16:
                case Type::Tag::UINT16:
                case Type::Tag::UINT8:
                case Type::Tag::INT8:
                case Type::Tag::FLOAT16:
                    LUISA_ERROR("Unsupported type.");
                    break;
                case Type::Tag::INT32:
                    arg_type_size += 4;
                    sb.append(luisa::format("int32_t {};\n", i.var_name));
                    break;
                case Type::Tag::UINT32:
                    arg_type_size += 4;
                    sb.append(luisa::format("uint32_t {};\n", i.var_name));
                    break;
                case Type::Tag::INT64:
                    arg_type_size = calc_align(arg_type_size, 8);
                    arg_type_size += 8;
                    sb.append(luisa::format("int64_t {};\n", i.var_name));
                    break;
                case Type::Tag::UINT64:
                    arg_type_size = calc_align(arg_type_size, 8);
                    arg_type_size += 8;
                    sb.append(luisa::format("uint64_t {};\n", i.var_name));
                    break;
                case Type::Tag::FLOAT32:
                    arg_type_size += 4;
                    sb.append(luisa::format("float {};\n", i.var_name));
                    break;
                case Type::Tag::FLOAT64:
                    arg_type_size = calc_align(arg_type_size, 8);
                    arg_type_size += 8;
                    sb.append(luisa::format("double {};\n", i.var_name));
                    break;
                case Type::Tag::VECTOR:
                case Type::Tag::ARRAY:
                case Type::Tag::MATRIX: {
                    sb << "::luisa::ArgType<";
                    get_type_name(get_type_name, sb, i.type, Usage::NONE);
                    sb << luisa::format(", 4, {}>", i.type->size()) << ' ' << i.var_name << ";\n";
                } break;
                case Type::Tag::STRUCTURE: {
                    arg_type_size = calc_align(arg_type_size, i.type->alignment());
                    arg_type_size += i.type->size();
                    sb << luisa::format("alignas({}) ", i.type->alignment());
                    get_type_name(get_type_name, sb, i.type, Usage::NONE);
                    sb << ' ' << i.var_name << ";\n";
                } break;
                default: break;
            }
        }
        sb.append("\t};\n");
    });
    func_map.emplace("ShaderBind", [&](vstd::StringBuilder &sb) {
        if (arg_type_size != 0) {
            sb << "\tSHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, _Global)\n";
        }
        sb << "\tSHADER_PARAMETER(FUintVector4, dsp_c)\n";
        auto get_usage_type = [](clangcxx::BuildArgument const &i) {
            return (luisa::to_underlying(i.var_usage) & luisa::to_underlying(Usage::WRITE)) != 0 ? "UAV" : "SRV";
        };
        auto get_usage_start = [](clangcxx::BuildArgument const &i) {
            return (luisa::to_underlying(i.var_usage) & luisa::to_underlying(Usage::WRITE)) != 0 ? "RW" : "";
        };
        for (auto &i : refl.kernel_args) {
            switch (i.type->tag()) {
                case Type::Tag::BUFFER:
                    sb << luisa::format("\tSHADER_PARAMETER_RDG_BUFFER_{}({}Buffer, ", get_usage_type(i), get_usage_start(i));
                    get_var_name(sb, i.type, i.resource_var_id);
                    sb << ")\n";
                    break;
                case Type::Tag::TEXTURE:
                    sb << luisa::format("\tSHADER_PARAMETER_RDG_TEXTURE_{}({}Texture{}D, ", get_usage_type(i), get_usage_start(i), i.type->dimension());
                    get_var_name(sb, i.type, i.resource_var_id);
                    sb << ")\n";
                    break;
                default:
            }
        }
    });
    func_map.emplace("declareDispatchArg", [&](vstd::StringBuilder &sb) {
        for (auto &i : refl.kernel_args) {
            sb << ", ";
            get_type_name(get_type_name, sb, i.type, i.var_usage);
            sb << " const& " << i.var_name;
        }
    });
    func_map.emplace("implDispatchArg", [&](vstd::StringBuilder &sb) {
        if (arg_type_size > 0) {
            sb << "\tArgs _args_;\n";
            for (auto &i : refl.kernel_args) {
                if (i.type->is_resource()) continue;
                switch (i.type->tag()) {
                    case Type::Tag::BOOL:
                        sb << "\t_args_." << i.var_name << " = " << i.var_name << " ? 1u : 0u;\n";
                        break;
                    default:
                        sb << "\t_args_." << i.var_name << " = " << i.var_name << ";\n";
                        break;
                }
            }
            sb << R"(
	auto buffer_desc = FRDGBufferDesc::CreateUploadDesc(sizeof(Args), 1);
	buffer_desc.Usage |= EBufferUsageFlags::StructuredBuffer;
	FRDGBufferRef _argBuffer_(graphBuilder->CreateBuffer(buffer_desc, TEXT("argBuffer")));
	FRDGUploadData<Args> _argBufferUpload_(*graphBuilder, 1);
	_argBufferUpload_[0] = _args_;
	graphBuilder->QueueBufferUpload(_argBuffer_, _argBufferUpload_, ERDGInitialDataFlags::None);
	params->_Global = graphBuilder->CreateSRV(_argBuffer_);
)"sv;
        }
        for (auto &i : refl.kernel_args) {
            if (!i.type->is_resource()) continue;
            sb << "\tparams->";
            get_var_name(sb, i.type, i.resource_var_id);
            sb << " = " << i.var_name << ";\n";
        }
    });
    func_map.emplace("Path", [&](vstd::StringBuilder &sb) {
        luisa::string p{local_virtual_path};
        for (auto &&i : p) {
            if (i == '\\') i = '/';
        }
        sb << p;
        if (p.empty() || p.back() != '/') {
            sb << '/';
        }
        sb << shader_name;
    });
    func_map.emplace("groupSize", [&](vstd::StringBuilder &sb) {
        sb << luisa::format("{}, {}, {}", refl.block_size.x, refl.block_size.y, refl.block_size.z);
    });
    {
        BinaryFileStream file_stream{luisa::to_string(header_template_path)};
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        codegen(sb, func_map, luisa::string_view{(char const *)data.data(), data.size()}, '$');
    }
    luisa::filesystem::path header_file_path{base_name};
    header_file_path.replace_extension(".h");
    auto header_file_path_str = luisa::to_string(header_file_path);
    for (auto &i : header_file_path_str) {
        if (i == '\\') i = '/';
    }
    {
        auto f = fopen(header_file_path_str.c_str(), "wb");
        if (f) {
            fwrite(sb.data(), sb.size(), 1, f);
            fclose(f);
        } else {
            LUISA_ERROR("Write to file {} failed.", header_file_path_str);
        }
    }
    sb.clear();
    {
        sb << "#include \"" << luisa::to_string(header_file_path.filename()) << "\"\n";
        BinaryFileStream file_stream{luisa::to_string(impl_template_path)};
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        codegen(sb, func_map, luisa::string_view{(char const *)data.data(), data.size()}, '$');
    }
    {
        luisa::filesystem::path impl_file_path{base_name};
        impl_file_path.replace_extension(".cpp");
        auto impl_file_path_str = luisa::to_string(impl_file_path);
        auto f = fopen(impl_file_path_str.c_str(), "wb");
        if (f) {
            fwrite(sb.data(), sb.size(), 1, f);
            fclose(f);
        } else {
            LUISA_ERROR("Write to file {} failed.", impl_file_path_str);
        }
    }
}