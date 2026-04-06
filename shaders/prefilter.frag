#version 450

#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform PushConstants
{
    int face;
    float roughness;
    int sourceRes;
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

    // make the simplifying assumption that V equals R equals the normal 
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, pc.roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V); //heres the sample

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            //fix this later
            // sample from the environment's mip level based on roughness/pdf to reduce spots
//            float D   = DistributionGGX(N, H, pc.roughness);
//            float NdotH = max(dot(N, H), 0.0);
//            float HdotV = max(dot(H, V), 0.0);
//            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001; 
//
//            float saTexel  = 4.0 * PI / (6.0 * pc.sourceRes * pc.sourceRes);
//            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
//                
//            float mipLevel = pc.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
            
            
            //prefilteredColor += textureLod(environmentMap, L, mipLevel).rgb * NdotL;

            prefilteredColor += texture(environmentMap, L).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;

    outColor = vec4(prefilteredColor, 1.0);
}