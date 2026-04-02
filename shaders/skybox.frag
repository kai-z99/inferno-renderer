#version 450
#extension GL_GOOGLE_include_directive : require

//set 0: per frame
#include "per_frame_layout.glsl"

//set 1: skybox cubemap
layout(set = 1, binding = 0) uniform samplerCube skyboxTex;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

void main()
{
    vec2 ndc = inUV * 2.0 - 1.0; //-> [-1,1]
    ndc.y = -ndc.y; //correct proj[1][1] *= -1
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewPos = inverse(sceneData.proj) * clip; 
    viewPos /= viewPos.w; //view space
    vec3 viewDir = normalize(viewPos.xyz);
    vec3 worldDir = normalize((inverse(sceneData.view) * vec4(viewDir, 0.0)).xyz); //world space

    vec3 color = texture(skyboxTex, worldDir).rgb;

    outFragColor = vec4(color, 1.0);
}