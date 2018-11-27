
// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D			gDiffuseMap	: register(t0);
Texture2D			gNormalMap	: register(t1);
TextureCube		gCubeMap		: register(t2);

SamplerState gsamPointWrap					: register(s0);
SamplerState gsamPointClamp				: register(s1);
SamplerState gsamLinearWrap				: register(s2);
SamplerState gsamLinearClamp				: register(s3);
SamplerState gsamAnisotropicWrap		: register(s4);
SamplerState gsamAnisotropicClamp	: register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbMaterial : register(b1)
{
	float4   gDiffuseAlbedo;
	float3   gFresnelR0;
	float    gRoughness;
	float4x4 gMatTransform;
};

cbuffer cbPass : register(b2)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light gLights[MaxLights];
};

cbuffer cbPlayer : register(b3)
{
	float4x4 gChaWorld;
	float4x4 gChaTexTransform;
	float4x4 gBoneTransforms[96];
};

cbuffer cbMonster : register(b4)
{
	float4x4 gMonsterWorld;
	float4x4 gMonsterTexTransform;
	float4x4 gMonsterBoneTransforms[96];
};

cbuffer cbUI : register(b5)
{
	float4x4 gUIWorld;
	float4x4 gUITexTransform;
	float gScale;
	float3 cbPerUIPad1;
};

cbuffer cbMonsterUI : register(b6)
{
	float4x4 gMonsterUIWorld;
	float4x4 gMonsterUITexTransform;
};
