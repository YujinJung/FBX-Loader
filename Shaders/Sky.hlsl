//=============================================================================
// Sky.fx by Frank Luna (C) 2011 All Rights Reserved.
//=============================================================================

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	// Always center sky about camera.
	posW.xyz += gEyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, gViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 litColor = gCubeMap.Sample(gsamLinearWrap, pin.PosL);
	
	/*float posX = gEyePosW.x * pin.PosL.x;
	float posZ = gEyePosW.z * pin.PosL.z;

	distanceToEye = posX < 0.0f ? 200.0f : distanceToEye;
	distanceToEye = posZ < 0.0f ? 200.0f : distanceToEye;*/

	/*float4 posW = mul(float4(pin.PosW, 1.0f), float4(0.05f, 0.05f, 0.05f, 1.0f));
	float distanceToEye = length(gEyePosW - posW.xyz);*/

	float distanceToEye = 210.0f;

	// TODO: fix
	float fogAmount = saturate((distanceToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);

	return litColor;
}

