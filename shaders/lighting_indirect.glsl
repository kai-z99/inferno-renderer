#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"

// 4.7.2 Energy loss in specular reflectance
vec3 ComputeSpecularEnergyCompensation(vec3 F0, float r)
{
    // dfg.y = envBRDF.A + envBRDF.B
    return vec3(1.0) + F0 * (1.0 / max(r, 1e-4) - 1.0);
}

// Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
// 5.6.2 Specular occlusion
float ComputeSpecularAO(float NoV, float ao, float roughness)
{
    return clamp(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

// account for possibility reflection vector points toward surface
// 5.6.2.1 Horizon specular occlusion
float ComputeHorizonOcclusion(vec3 R, vec3 N)
{
    return pow(min(1.0 + dot(R, N), 1.0), 2.0);
}

// ------------------------------------------------------------
// Base lobes
// ------------------------------------------------------------

vec3 EvalBaseDiffuseIBL(
    SurfaceData sd,
    samplerCube irradianceMap)
{
    // 4.8.3.1 Base color remapping
    vec3 diffuseColor = sd.albedo * (1.0 - sd.metallic);

    vec3 irradiance = texture(irradianceMap, sd.N).rgb;
    vec3 diffuseIBL = diffuseColor * irradiance;

    // 5.6.1 Diffuse occlusion
    diffuseIBL *= sd.ao;

    return diffuseIBL;
}

vec3 EvalBaseSpecularIBL(
    SurfaceData sd,
    samplerCube prefilterMap,
    sampler2D brdfLUT,
    float prefilterMaxLod)
{
    //radiance integral
    vec3 prefilteredColor = textureLod(prefilterMap, sd.R, sd.roughness * prefilterMaxLod).rgb;

    //brdf integral split sum A and B
    vec2 envBRDF = texture(brdfLUT, vec2(sd.NdotV, sd.roughness)).rg;

    vec3 specularIBL = prefilteredColor * (sd.F0 * envBRDF.x + envBRDF.y);

    specularIBL *= ComputeSpecularAO(sd.NdotV, sd.ao, sd.roughness);
    specularIBL *= ComputeSpecularEnergyCompensation(sd.F0, envBRDF.x + envBRDF.y);
    specularIBL *= ComputeHorizonOcclusion(sd.R, sd.N);

    return specularIBL;
}

// ------------------------------------------------------------
// Diffuse Transmission
// ------------------------------------------------------------

// vec3 EvalBaseDiffuseReflectIBL(
//     SurfaceData sd,
//     samplerCube irradianceMap)
// {
//     vec3 diffuseReflectColor =
//         (1.0 - sd.diffuseTransmissionFactor) *
//         sd.albedo *
//         (1.0 - sd.metallic);

//     vec3 irradiance = texture(irradianceMap, sd.N).rgb;
//     return diffuseReflectColor * irradiance * sd.ao;
// }

// vec3 EvalBaseDiffuseTransmitIBL(
//     SurfaceData sd,
//     samplerCube irradianceMap)
// {
//     vec3 diffuseTransmitColor = 
//         sd.diffuseTransmissionFactor * 
//         sd.diffuseTransmissionColor * 
//         (1.0 - sd.metallic);

//     // thin-surface approximation:
//     // transmitted diffuse comes from the opposite hemisphere
//     vec3 transmittedIrradiance = texture(irradianceMap, sd.N).rgb;
//     return diffuseTransmitColor * transmittedIrradiance;
// }

// ------------------------------------------------------------
// Clearcoat
// ------------------------------------------------------------

vec3 EvalClearCoatIBL(
    SurfaceData sd, 
    samplerCube prefilterMap,
    sampler2D brdfLUT,
    float prefilterMaxLod,
    inout vec3 baseDiffuse,
    inout vec3 baseSpecular)
{
    float clearcoatNdotV = max(dot(sd.V, sd.clearcoatNormal),0.0);
    vec3 clearcoatR = reflect(-sd.V, sd.clearcoatNormal);
    float Fc = FresnelSchlick(clearcoatNdotV, vec3(0.04)).r * sd.clearcoatFactor;
    float attenuation = 1.0 - Fc;

    baseDiffuse *= attenuation;
    baseSpecular *= attenuation;

    float ao = ComputeSpecularAO(clearcoatNdotV, sd.ao, sd.clearcoatRoughness);

    vec3 clearcoat = textureLod(prefilterMap, clearcoatR, sd.clearcoatRoughness * prefilterMaxLod).rgb * Fc * ao;

    return clearcoat;
}

// ------------------------------------------------------------
// Orchestrator
// ------------------------------------------------------------

vec3 EvalIndirectLighting(
    SurfaceData sd,
    samplerCube irradianceMap,
    samplerCube prefilterMap,
    sampler2D brdfLUT,
    float prefilterMaxLod)
{
    vec3 baseDiffuse = EvalBaseDiffuseIBL(sd, irradianceMap);
    vec3 baseSpecular = EvalBaseSpecularIBL(sd, prefilterMap, brdfLUT, prefilterMaxLod);
    vec3 clearcoat = EvalClearCoatIBL(sd, prefilterMap, brdfLUT, prefilterMaxLod, baseDiffuse, baseSpecular);

    return baseDiffuse + baseSpecular + clearcoat;
}