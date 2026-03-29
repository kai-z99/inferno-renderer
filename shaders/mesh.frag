#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPosWorld;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 lightCol = sceneData.sunlightColor.xyz;
	float lightPower = sceneData.sunlightDirection.w;
	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
	vec3 viewDir =  normalize(inFragPosWorld - sceneData.camPos.xyz);
	vec3 normal = normalize(inNormal);
	vec3 albedo = texture(colorTex, inUV).rgb * materialData.colorFactors.rgb;
	float metallic = texture(metalRoughTex, inUV).b * materialData.metal_rough_factors.x;
	float roughness = texture(metalRoughTex, inUV).g * materialData.metal_rough_factors.y;

	outFragColor = vec4(sceneData.ambientColor.xyz * albedo + DirLightEval_CookTorrance(lightCol, lightPower, lightDir, viewDir, normal, albedo, metallic, roughness), 1.0);
	
	//outFragColor = vec4(texture(shadowMap, inUV));
	//outFragColor = vec4(albedo, 1.0);
	//outFragColor = vec4(normalize(inNormal), 1.0);
}