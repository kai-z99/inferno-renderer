#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"


//Directions/normal are assumed to be normalized!
vec3 DirLightEval_CookTorrance(vec3 lightCol, float lightPower, vec3 lightDir, vec3 viewDir, vec3 normal, vec3 albedo, float metallic, float roughness)
{
    const float PI = 3.14159265359;

    //0.04 is acceptable for most materials
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    vec3 halfway = normalize(-viewDir + -lightDir); //micro facet normal

    vec3 radiance = lightCol * lightPower;
    vec3 fLambert = albedo / PI;

    //cook-torrence BRDF
    float NDF = DistributionGGX(normal, halfway, roughness);
    float G = GeometrySmith(normal, -viewDir, -lightDir, roughness);
    vec3 F = FresnelSchlick(max(dot(halfway, -viewDir), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(-viewDir, normal), 0.0) * max(dot(-lightDir, normal), 0.0) + 0.00001;
    vec3 fCookTorrence = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; //kd theoretically either exists or is 0. (binary metal values)

    float NdotL = max(dot(normal, -lightDir),0.0f);

    return (kD * fLambert + fCookTorrence) * radiance * NdotL;
}