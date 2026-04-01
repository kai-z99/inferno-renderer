#version 450

#extension GL_GOOGLE_include_directive : require
#include "lighting.glsl"
#include "pcss.glsl"

//set 0: per frame descriptor set bindings
#include "per_frame_layout.glsl"

//set 1: shadow map
layout(set = 1, binding = 0) uniform sampler2D shadowMap;

//set 2: materials
layout(set = 2, binding = 0) uniform GLTFMaterialData
{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;
layout(set = 2, binding = 1) uniform sampler2D colorTex;
layout(set = 2, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 2, binding = 3) uniform sampler2D normalTex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inFragPosWorld;
layout (location = 4) in vec4 inTangent;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 lightCol = sceneData.sunlightColor.xyz;
	float lightPower = sceneData.sunlightDirection.w;
	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
	vec3 viewDir =  normalize(inFragPosWorld.xyz - sceneData.camPos.xyz);

	vec3 T = normalize(inTangent.xyz);
	vec3 N = normalize(inNormal);
	vec3 B = normalize(cross(N, T) * inTangent.w);
	mat3 TBN = mat3(T, B, N);
	vec3 normalTangentSpace = texture(normalTex, inUV).xyz;
	vec3 normal = normalize(TBN * normalTangentSpace);

	vec3 albedo = texture(colorTex, inUV).rgb * materialData.colorFactors.rgb;
	float metallic = texture(metalRoughTex, inUV).b * materialData.metal_rough_factors.x;
	float roughness = texture(metalRoughTex, inUV).g * materialData.metal_rough_factors.y;

	float shadow = ShadowEval_PCF(shadowMap, inFragPosWorld, sceneData.lightViewProj);
	outFragColor = vec4(sceneData.ambientColor.xyz * albedo + shadow * DirLightEval_CookTorrance(lightCol, lightPower, lightDir, viewDir, normal, albedo, metallic, roughness), 1.0);
	
	//outFragColor = vec4(texture(shadowMap, inUV));
	//outFragColor = vec4(albedo, 1.0);
	//outFragColor = vec4(normalize(inNormal), 1.0);
}