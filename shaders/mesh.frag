#version 450

#extension GL_GOOGLE_include_directive : require
#include "lighting_direct.glsl"
#include "lighting_indirect.glsl"
#include "pcss.glsl"

//set 0: per frame descriptor set bindings
#include "per_frame_layout.glsl"

//set 1: lighting resources like shadowmap, cubemaps
layout(set = 1, binding = 0) uniform sampler2D shadowMap;
layout(set = 1, binding = 1) uniform samplerCube irradianceCubemap;
layout(set = 1, binding = 2) uniform samplerCube prefilterCubemap;
layout(set = 1, binding = 3) uniform sampler2D brdfLUT;

//set 2: materials
layout(set = 2, binding = 0) uniform GLTFMaterialData
{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;
layout(set = 2, binding = 1) uniform sampler2D colorTex;
layout(set = 2, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 2, binding = 3) uniform sampler2D normalTex;
layout(set = 2, binding = 4) uniform sampler2D emissiveTex;
layout(set = 2, binding = 5) uniform sampler2D aoTex;

//in
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inFragPosWorld;
layout (location = 4) in vec4 inTangent;
//out
layout (location = 0) out vec4 outFragColor;

void main() 
{
	// GET DATA ---------------------
	SurfaceData sd;
	sd.P = inFragPosWorld.xyz;
	sd.N = normalize(inNormal);
	if (length(inTangent.xyz) > 0.0)
	{
		vec3 T = normalize(inTangent.xyz);
		vec3 B = normalize(cross(sd.N, T) * inTangent.w);
		mat3 TBN = mat3(T, B, sd.N);
		vec3 normalTangentSpace = texture(normalTex, inUV).xyz * 2.0 - 1.0;
		sd.N = normalize(TBN * normalTangentSpace);
	}
	sd.V = normalize(sceneData.camPos.xyz - inFragPosWorld.xyz);
	sd.albedo =  texture(colorTex, inUV).rgb * materialData.colorFactors.rgb;
	vec3 metalRough = texture(metalRoughTex, inUV).rgb;
	sd.metallic = metalRough.b * materialData.metal_rough_factors.x;
	sd.roughness = metalRough.g * materialData.metal_rough_factors.y;
	sd.ao = texture(aoTex, inUV).r;
	sd.emissive = texture(emissiveTex, inUV).xyz;
	sd.F0 = mix(vec3(0.04), sd.albedo, sd.metallic);
	sd.NdotV = max(dot(sd.N, sd.V), 0.0);
	sd.R = reflect(-sd.V, sd.N);

	vec3 lightCol = sceneData.sunlightColor.xyz;
	float lightPower = sceneData.sunlightDirection.w;
	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);

	// DIRECT LIGHT ----------------

	float shadow = ShadowEval_PCF(shadowMap, inFragPosWorld, sceneData.lightViewProj);
	vec3 direct = shadow * EvalDirectLighting(sd, lightCol, lightPower, lightDir);

	// INDIRECT LIGHT --------------
	
	// diffuse IBL
	vec3 indirect = EvalIndirectLighting(sd, irradianceCubemap, prefilterCubemap, brdfLUT, sceneData.prefilterMaxLod );
	indirect *= sceneData.iblIntensity;
	
	outFragColor = vec4(direct + indirect + sd.emissive, 1.0);
	//outFragColor = vec4(direct, 1.0);
	//outFragColor = vec4(texture(shadowMap, inUV));
	//outFragColor = vec4(albedo, 1.0);
	//outFragColor = vec4(normalize(normal), 1.0);
}