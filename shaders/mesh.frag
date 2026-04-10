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
	vec4 diffuse_transmission_factors; //color on xyz, factor on w
	vec4 clearcoat_factors;            // x: clearcoatFactor, y: clearcoatRoughnessFactor

} materialData;
layout(set = 2, binding = 1) uniform sampler2D colorTex;
layout(set = 2, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 2, binding = 3) uniform sampler2D normalTex;
layout(set = 2, binding = 4) uniform sampler2D emissiveTex;
layout(set = 2, binding = 5) uniform sampler2D aoTex;
layout(set = 2, binding = 6) uniform sampler2D diffuseTransmissionColorTex;
layout(set = 2, binding = 7) uniform sampler2D diffuseTransmissionFactorTex;
layout(set = 2, binding = 8) uniform sampler2D clearcoatTex;
layout(set = 2, binding = 9) uniform sampler2D clearcoatRoughnessTex;

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
	sd.roughness = max(metalRough.g * materialData.metal_rough_factors.y, 0.045); //4.8.3.3 Roughness remapping and clamping
	sd.ao = texture(aoTex, inUV).r;
	sd.emissive = texture(emissiveTex, inUV).xyz;
	sd.F0 = mix(vec3(0.04), sd.albedo, sd.metallic);
	sd.NdotV = max(dot(sd.N, sd.V), 0.0);
	sd.R = reflect(-sd.V, sd.N);

	//KHR_materials_diffuse_transmission
	sd.diffuseTransmissionColor = texture(diffuseTransmissionColorTex, inUV).rgb * materialData.diffuse_transmission_factors.xyz;
	sd.diffuseTransmissionFactor = texture(diffuseTransmissionFactorTex, inUV).a * materialData.diffuse_transmission_factors.w;

	//KHR_materials_clearcoat
	sd.clearcoatNormal = sd.N;
	sd.clearcoatFactor = texture(clearcoatTex, inUV).r * materialData.clearcoat_factors.x;
	sd.clearcoatRoughness = texture(clearcoatRoughnessTex, inUV).g * materialData.clearcoat_factors.y;

	// DIRECT LIGHT ----------------

	vec3 lightCol = sceneData.sunlightColor.xyz;
	float lightPower = sceneData.sunlightDirection.w;
	vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);

	vec3 direct = EvalDirectLighting(sd, lightCol, lightPower, lightDir);
	direct *= ShadowEval_PCF(shadowMap, inFragPosWorld, sceneData.lightViewProj);


	// INDIRECT LIGHT --------------
	
	vec3 indirect = EvalIndirectLighting(sd, irradianceCubemap, prefilterCubemap, brdfLUT, sceneData.prefilterMaxLod );
	indirect *= sceneData.iblIntensity; //artist tweak 
	
	outFragColor = vec4(direct + indirect + sd.emissive, 1.0);

	//outFragColor = vec4(vec3(sd.diffuseTransmissionFactor), 1.0);
	//outFragColor = vec4(vec3(sd.roughness), 1.0);
	//outFragColor = vec4(albedo, 1.0);
	//outFragColor = vec4(normalize(normal), 1.0);
}