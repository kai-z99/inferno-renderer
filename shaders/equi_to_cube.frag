#version 450
#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

layout(location = 0) in vec2 inUV;   // from fullscreen.vert
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push
{
    int faceIndex; // 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z
} pc;

const vec2 invAtan = vec2(0.15915494, 0.31830989); // 1/(2pi), 1/pi

vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

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
    vec3 dir = faceUvToDir(pc.faceIndex, inUV);
    vec2 uv  = sampleSphericalMap(dir);
    vec3 c   = texture(equirectangularMap, uv).rgb;
    outColor = vec4(c, 1.0);
}