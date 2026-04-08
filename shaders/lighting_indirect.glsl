#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"

vec3 EvalIndirectLighting(
    SurfaceData sd,
    samplerCube irradianceMap,
    samplerCube prefilterMap,
    sampler2D brdfLUT,
    float prefilterMaxLod)
{
	vec3 diffuseColor = sd.albedo * (1.0 - sd.metallic);
	vec3 irradiance = texture(irradianceMap, sd.N).rgb;
	vec3 diffuseIBL = diffuseColor * irradiance; 
	diffuseIBL *= sd.ao;							//ao

	// specular IBL
	vec3 prefilteredColor = textureLod(prefilterMap, sd.R, sd.roughness * prefilterMaxLod).rgb;
	vec2 envBRDF = texture(brdfLUT, vec2(sd.NdotV, sd.roughness)).rg; //split sum A and B
	vec3 specularIBL = prefilteredColor * (sd.F0 * envBRDF.x + envBRDF.y); //filament uses F0 not kS
	vec3 energyCompensation = vec3(1.0) + sd.F0 * (1.0 / max(envBRDF.x + envBRDF.y, 1e-4) - 1.0); //dfg.y = envBRDF.A + envBRDF.B
	specularIBL *= energyCompensation;
	specularIBL *= ComputeSpecularAO(sd.NdotV, sd.ao, sd.roughness); //ao
	specularIBL *= pow(min(1.0 + dot(sd.R, sd.N), 1.0), 2.0); // ao correction from filament 

    return diffuseIBL + specularIBL;
}