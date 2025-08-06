#include "RenderGraphUtils.h"
IMPLEMENT_SHADER_TYPE(, $ClsName, TEXT("$Path"), TEXT("main"), SF_Compute);
void $ClsDspName::dispatch(uint32_t dispatch_x, uint32_t dispatch_y, uint32_t dispatch_z$declareDispatchArg)
{
	$ClsName::FParameters* params;
	params = graphBuilder->AllocParameters<$ClsName::FParameters>();
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
    params->dsp_c = FUintVector4{dispatch_x, dispatch_y, dispatch_z, 0u};
	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<$ClsName> ComputeShader(GlobalShaderMap);
	const FIntVector3 groupSize{$groupSize};
	auto calc_align = [](int c, int a)-> int
	{
		return (c + a - 1) & (~(a - 1));
	};
	FIntVector3 groupCount{
		calc_align(dispatch_x, groupSize.X),
		calc_align(dispatch_y, groupSize.Y),
		calc_align(dispatch_z, groupSize.Z),
	};
	FComputeShaderUtils::AddPass(*graphBuilder, FRDGEventName(TEXT("Compute Pass")), ERDGPassFlags::Compute, ComputeShader, params, groupCount);
}