#version 450
#extension GL_GOOGLE_include_directive : require

//set 0: per frame
#include "per_frame_layout.glsl"

//set 1: skybox texture
layout(set = 1, binding = 0) uniform sampler2D skyboxTex;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

vec2 dir_to_equirect_uv(vec3 dir)
{
    dir = normalize(dir);
    float phi = atan(dir.z, dir.x);
    float theta = asin(clamp(dir.y, -1.0, 1.0));
    return vec2
    (
        phi / (2.0 * 3.14159265) + 0.5,
        theta / 3.14159265 + 0.5
    );
}

void main()
{
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewPos = inverse(sceneData.proj) * clip;
    viewPos /= viewPos.w;
    vec3 viewDir = normalize(viewPos.xyz);
    vec3 worldDir = normalize((inverse(sceneData.view) * vec4(viewDir, 0.0)).xyz);
    vec2 uv = dir_to_equirect_uv(worldDir);
    vec3 color = texture(skyboxTex, uv).rgb;
    outFragColor = vec4(color, 1.0);
}