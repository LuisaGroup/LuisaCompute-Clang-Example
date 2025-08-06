#pragma once
#include <stdint.h>
#include <array>
#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
#include "LuisaShaderBase.h"
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
	$ClsDspName(FRDGBuilder& graphBuilder)
		: graphBuilder(&graphBuilder),
		  shader([this]()
		  {
			  const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			  return TShaderMapRef<$ClsName>(GlobalShaderMap);
		  }())
	{
	}
	void dispatch(uint32_t dispatch_x, uint32_t dispatch_y, uint32_t dispatch_z$declareDispatchArg);
};