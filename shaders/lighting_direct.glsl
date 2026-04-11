#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"

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
// Diffuse transmission
// ------------------------------------------------------------

// vec3 EvalBaseDiffuseReflectDirect(SurfaceData sd)
// {
//     vec3 diffuseReflectColor =
// 	    sd.albedo *
//         (1.0 - sd.diffuseTransmissionFactor) *
//         (1.0 - sd.metallic);

//     return diffuseReflectColor * (1.0 / PI);
// }

// vec3 EvalBaseDiffuseTransmitDirect(SurfaceData sd)
// {
//     vec3 diffuseTransmitColor =
// 	    sd.diffuseTransmissionColor *
//         sd.diffuseTransmissionFactor *
//         (1.0 - sd.metallic);

//     return diffuseTransmitColor * (1.0 / PI);
// }

// ------------------------------------------------------------
// Clearcoat
// ------------------------------------------------------------

vec3 EvalClearcoatDirect(
    SurfaceData sd,
    vec3 L,
    inout vec3 baseDiffuse,
    inout vec3 baseSpecular)
{
    vec3 H = normalize(sd.V + L);

    float Dc = DistributionGGX(sd.clearcoatNormal, H, sd.clearcoatRoughness);
    float Vc = V_Kelemen(max(dot(L, H), 0.0));
    float Fc = FresnelSchlick(max(dot(H, sd.V), 0.0), vec3(0.04)).r * sd.clearcoatFactor;
    float Frc = Dc * Vc * Fc;

    baseDiffuse *= (1.0 - Fc);
    baseSpecular *= (1.0 - Fc);

    return vec3(Frc);
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
    float NdotL =      max(dot(sd.N, L), 0.0);
    //float NdotLBack  = max(dot(-sd.N, L), 0.0);

    vec3 radiance = lightCol * lightPower;

    vec3 direct = vec3(0.0);

    //for simplicity im gonna make an assumption that NdotL approx= ccNdotL so i dont have to do more branching logic for now
    if (NdotL > 0.0)
    {
        vec3 baseDiffuse = EvalBaseDiffuseDirect(sd);
        vec3 baseSpecular = EvalBaseSpecularDirect(sd, L);
        vec3 clearcoat = EvalClearcoatDirect(sd, L, baseDiffuse, baseSpecular);
        vec3 total = baseDiffuse + baseSpecular + clearcoat;
        direct += total * radiance * NdotL;
    }

    // opposite hemisphere diffuse transmission only
    // if (NdotLBack > 0.0)
    // {
    //     vec3 back = EvalBaseDiffuseTransmitDirect(sd);
    //     direct += back * radiance * NdotLBack;
    // }

    return direct;
}