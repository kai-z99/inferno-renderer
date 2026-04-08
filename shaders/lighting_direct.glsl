#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"

const float PI = 3.14159265359;

// ------------------------------------------------------------
// Base lobes
// ------------------------------------------------------------

vec3 EvalBaseDiffuseDirect(SurfaceData sd)
{
    // 4.8.3.1 Base color remapping
    vec3 diffuseColor = sd.albedo * (1.0 - sd.metallic);
    return diffuseColor * (1.0 / PI);
}

vec3 EvalBaseSpecularDirect(
    SurfaceData sd,
    vec3 L)
{
    vec3 H = normalize(sd.V + L);

    float D = DistributionGGX(sd.N, H, sd.roughness);
    float G = GeometrySmith(sd.N, sd.V, L, sd.roughness);
    vec3 F = FresnelSchlick(max(dot(H, sd.V), 0.0), sd.F0); //handles kS

    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(sd.N, sd.V), 0.0) * max(dot(sd.N, L), 0.0) + 0.00001;

    return numerator / denominator;
}

// ------------------------------------------------------------
// orchestrator
// ------------------------------------------------------------
vec3 EvalDirectLighting(
    SurfaceData sd,
    vec3 lightCol,
    float lightPower,
    vec3 lightDir)
{
    vec3 L = normalize(-lightDir);
    float NdotL = max(dot(sd.N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    vec3 radiance = lightCol * lightPower;

    vec3 direct = vec3(0.0);

    // base layer
    direct += EvalBaseDiffuseDirect(sd);
    direct += EvalBaseSpecularDirect(sd, L);

    return direct * radiance * NdotL;
}