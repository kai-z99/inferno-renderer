#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform PushConstants
{
    int face;
} pc;

const float PI = 3.14159265359;

vec3 faceUvToDir(int face, vec2 uv)
{
    // uv [0,1] -> [-1,1]
    vec2 a = uv * 2.0 - 1.0;
    float x = a.x;
    float y = a.y;

    if (face == 0) return normalize(vec3( 1.0, -y, -x)); // +X
    if (face == 1) return normalize(vec3(-1.0, -y,  x)); // -X
    if (face == 2) return normalize(vec3( x, 1.0,  y));  // +Y
    if (face == 3) return normalize(vec3( x, -1.0, -y)); // -Y
    if (face == 4) return normalize(vec3( x, -y,  1.0)); // +Z
                   return normalize(vec3(-x, -y, -1.0)); // -Z
}

void main()
{
    vec3 N = faceUvToDir(pc.face, inUV);

    vec3 irradiance = vec3(0.0);

    // Build tangent basis
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0)
                               : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float sampleDelta = 0.01;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );

            vec3 sampleVec =
                tangentSample.x * right +
                tangentSample.y * up +
                tangentSample.z * N;

            irradiance += texture(environmentMap, sampleVec).rgb
                        * cos(theta) * sin(theta);
            nrSamples += 1.0;
        }
    }

    irradiance = PI * irradiance / nrSamples;
    outColor = vec4(irradiance, 1.0);
}