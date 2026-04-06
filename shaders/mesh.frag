#version 450

#extension GL_GOOGLE_include_directive : require
#include "lighting.glsl"
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
	vec3 lightCol = sceneData.sunlightColor.xyz;
	float lightPower = sceneData.sunlightDirection.w;
	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
	vec3 viewDir =  normalize(inFragPosWorld.xyz - sceneData.camPos.xyz);

	vec3 N = normalize(inNormal);
	vec3 normal = N;
	if (length(inTangent.xyz) > 0.0)
	{
		vec3 T = normalize(inTangent.xyz);
		vec3 B = normalize(cross(N, T) * inTangent.w);
		mat3 TBN = mat3(T, B, N);
		vec3 normalTangentSpace = texture(normalTex, inUV).xyz * 2.0 - 1.0;
		normal = normalize(TBN * normalTangentSpace);
	}

	vec3 albedo = texture(colorTex, inUV).rgb * materialData.colorFactors.rgb;
	float metallic = texture(metalRoughTex, inUV).b * materialData.metal_rough_factors.x;
	float roughness = texture(metalRoughTex, inUV).g * materialData.metal_rough_factors.y;

	float shadow = ShadowEval_PCF(shadowMap, inFragPosWorld, sceneData.lightViewProj);
	vec3 direct = shadow * DirLightEval_CookTorrance(lightCol, lightPower, lightDir, viewDir, normal, albedo, metallic, roughness);

	vec3 V = normalize(sceneData.camPos.xyz - inFragPosWorld.xyz);
	float NdotV = max(dot(normal, V), 0.0);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 kS = FresnelSchlick(NdotV, F0);
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

	// diffuse IBL
	vec3 irradiance = texture(irradianceCubemap, normal).rgb;
	vec3 diffuseIBL = kD * albedo * irradiance;

	// specular IBL
	vec3 R = reflect(-V, normal);

	vec3 prefilteredColor = textureLod(prefilterCubemap, R, roughness * sceneData.prefilterMaxLod).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
	vec3 specularIBL = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

	vec3 ambient = (diffuseIBL + specularIBL) * sceneData.iblIntensity;
	outFragColor = vec4(direct + ambient, 1.0);
	//outFragColor = vec4(direct, 1.0);
	//outFragColor = vec4(texture(shadowMap, inUV));
	//outFragColor = vec4(albedo, 1.0);
	//outFragColor = vec4(normalize(normal), 1.0);
}