#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"

//4.7.2 Energy loss in specular reflectance
vec3 ComputeSpecularEnergyCompensation(vec3 F0, float r)
{
    //dfg.y = envBRDF.A + envBRDF.B
    return vec3(1.0) + F0 * (1.0 / max(r, 1e-4) - 1.0);
}

// Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
//5.6.2 Specular occlusion
float ComputeSpecularAO(float NoV, float ao, float roughness) 
{
    return clamp(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

//account for possibiliy reflection vector pointing towards surface
//5.6.2.1 Horizon specular occlusion
float ComputeHorizonOcclusion(vec3 R, vec3 N)
{
    return pow(min(1.0 + dot(R, N), 1.0), 2.0);
}


vec3 EvalIndirectLighting(
    SurfaceData sd,
    samplerCube irradianceMap,
    samplerCube prefilterMap,
    sampler2D brdfLUT,
    float prefilterMaxLod)
{
    // diffuse ------------------------
    //4.8.3.1 Base color remapping
	vec3 diffuseColor = sd.albedo * (1.0 - sd.metallic);
	vec3 irradiance = texture(irradianceMap, sd.N).rgb;
	vec3 diffuseIBL = diffuseColor * irradiance; 
    diffuseIBL *= sd.ao; //5.6.1 Diffuse occlusion

	// specular ------------------------
	vec3 prefilteredColor = textureLod(prefilterMap, sd.R, sd.roughness * prefilterMaxLod).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(sd.NdotV, sd.roughness)).rg; //split sum A and B
	vec3 specularIBL = prefilteredColor * (sd.F0 * envBRDF.x + envBRDF.y); //filament uses F0 not kS
	specularIBL *= ComputeSpecularEnergyCompensation(sd.F0, envBRDF.x + envBRDF.y);
	specularIBL *= ComputeSpecularAO(sd.NdotV, sd.ao, sd.roughness); //ao
	specularIBL *= ComputeHorizonOcclusion(sd.R, sd.N);// ao correction from filament 

    return diffuseIBL + specularIBL;
}