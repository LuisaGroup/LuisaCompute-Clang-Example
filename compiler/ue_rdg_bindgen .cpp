#include "ue_rdg_bindgen.h"
#include <luisa/core/logging.h>
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
//////// header
/*
#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
class $ClsName : public FGlobalShader
{
public:
$ArgType
	DECLARE_EXPORTED_SHADER_TYPE($ClsName, Global,);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_0)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_1)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_2)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_3)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_4)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_5)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_6)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_7)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_8)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_9)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_10)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_11)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_12)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_13)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_14)
	SHADER_PARAMETER_SAMPLER(SamplerState, s_15)
$ShaderBind
	END_SHADER_PARAMETER_STRUCT()
	SHADER_USE_PARAMETER_STRUCT($ClsName, FGlobalShader);
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

struct $ClsDspName
{
    using Args = $ClsName::Args;
	FRDGBuilder* graphBuilder;
	TShaderMapRef<$ClsName> shader;
	$ClsName::FParameters* params;
	FRDGBufferRef argBuffer;
    static FRDGBufferRef UploadArg(FRDGBuilder& GraphBuilder, Args const& args);
	$ClsDspName(FRDGBuilder& graphBuilder)
		: graphBuilder(&graphBuilder),
		  shader([this]()
		  {
			  const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			  return TShaderMapRef<$ClsName>(GlobalShaderMap);
		  }())
	{
		params = graphBuilder.AllocParameters<$ClsName::FParameters>();
	}
	void dispatch(FIntVector3 const& threadCount $declareDispatchArg);
};
*/
//////////// cpp
/*
IMPLEMENT_SHADER_TYPE(, $ClsName, TEXT("$Path"), TEXT("main"), SF_Compute);
void $ClsDspName::dispatch(FIntVector3 const& threadCount $declareDispatchArg)
{
$implDispatchArg
	params->s_0 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	params->s_1 = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	params->s_2 = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	params->s_3 = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	params->s_4 = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	params->s_5 = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	params->s_6 = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	params->s_7 = TStaticSamplerState<SF_AnisotropicLinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	params->s_8 = TStaticSamplerState<SF_Point, AM_Mirror, AM_Mirror, AM_Mirror>::GetRHI();
	params->s_9 = TStaticSamplerState<SF_Bilinear, AM_Mirror, AM_Mirror, AM_Mirror>::GetRHI();
	params->s_10 = TStaticSamplerState<SF_Trilinear, AM_Mirror, AM_Mirror, AM_Mirror>::GetRHI();
	params->s_11 = TStaticSamplerState<SF_AnisotropicLinear, AM_Mirror, AM_Mirror, AM_Mirror>::GetRHI();
	params->s_12 = TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Border>::GetRHI();
	params->s_13 = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	params->s_14 = TStaticSamplerState<SF_Trilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	params->s_15 = TStaticSamplerState<SF_AnisotropicLinear, AM_Border, AM_Border, AM_Border>::GetRHI();
    // params->_Global = graphBuilder->CreateSRV(argBuffer);
    params->dsp_c = FUintVector4{(uint32_t)threadCount.X, (uint32_t)threadCount.Y, (uint32_t)threadCount.Z, 0u};
	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<$ClsName> ComputeShader(GlobalShaderMap);
	const FIntVector3 groupSize{$groupSize};
	auto calc_align = [](int c, int a)-> int
	{
		return (c + a - 1) & (~(a - 1));
	};
	FIntVector3 groupCount{
		calc_align(threadCount.X, groupSize.X),
		calc_align(threadCount.Y, groupSize.Y),
		calc_align(threadCount.Z, groupSize.Z),
	};
	FComputeShaderUtils::AddPass(*graphBuilder,
	                             FRDGEventName(TEXT("Blur Compute Pass")),
	                             ERDGPassFlags::Compute, ComputeShader,
	                             params, groupCount);
}
FRDGBufferRef $ClsDspName::UploadArg(FRDGBuilder& GraphBuilder, Args const& args)
{
    if constexpr (sizeof(Args) > 1) {
        FRDGBufferRef argBuffer = GraphBuilder.CreateBuffer(
        FRDGBufferDesc::CreateUploadDesc(sizeof(Args), 1),
        TEXT("$ArgBufferName"));
        FRDGUploadData<Args> argBufferUpload(GraphBuilder, 1);
        argBufferUpload[0] = args;
        GraphBuilder.QueueBufferUpload(argBuffer, argBufferUpload, ERDGInitialDataFlags::None);
        return argBuffer;
    } else {
        return {};
    }
}
*/
