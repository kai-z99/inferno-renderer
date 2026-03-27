#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inPosWorld;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 test = DirLightEval_CookTorrance(vec3(1.0), 1.0, vec3(1.0),vec3(1.0),vec3(1.0),vec3(1.0), 1.0, 1.0);


	float d = max(dot(normalize(inNormal), -sceneData.sunlightDirection.xyz), 0.0);
	float s = pow(max(dot(-normalize(inPosWorld - sceneData.camPos.xyz), reflect(sceneData.sunlightDirection.xyz, normalize(inNormal))), 0.0), 64.0);
	vec3 a = sceneData.ambientColor.xyz;

	vec3 I_l = sceneData.sunlightColor.xyz;
	float specMap = texture(metalRoughTex, inUV).x;
	vec3 colorMap = texture(colorTex, inUV).xyz;

	outFragColor = vec4(a*colorMap + d*I_l*colorMap + s*I_l*specMap + test, 1.0f);
	//outFragColor = vec4(normalize(inNormal), 1.0);
}