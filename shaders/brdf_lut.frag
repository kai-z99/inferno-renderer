#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "sampling.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec2 outColor;

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    //resolve the view vector  (in local coords)
    vec3 V;
    V.x = sqrt(1.0 - NdotV*NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0; 

    vec3 N = vec3(0.0, 0.0, 1.0); //define an up, since we are working with n dot v a scalar angle
    
    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generates a sample vector that's biased towards the
        // preferred alignment direction (importance sampling).
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V); //sample vector

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness); //luckily, this function on depends on n dot v (n relative to v), not true V. 
            //note: dot(n,l) is in G too.
            float G_Vis = (G * VdotH) / (NdotH * NdotV); 
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    //return the intergal in the form F0*A + B
    return vec2(A, B);
        
}
// ----------------------------------------------------------------------------
void main() 
{
    vec2 integratedBRDF = IntegrateBRDF(inUV.x, inUV.y);
    outColor = integratedBRDF;
}
