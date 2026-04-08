#extension GL_GOOGLE_include_directive : require
#include "pbr_common.glsl"
#include "surface.glsl"


//Directions/normal are assumed to be normalized!
vec3 EvalDirectLighting(SurfaceData sd, vec3 lightCol, float lightPower, vec3 lightDir)
{
    const float PI = 3.14159265359;
    
    vec3 halfway = normalize(sd.V + -lightDir); //micro facet normal

    vec3 radiance = lightCol * lightPower;
    vec3 fLambert = sd.albedo / PI;

    //cook-torrence BRDF
    float NDF = DistributionGGX(sd.N, halfway, sd.roughness);
    float G = GeometrySmith(sd.N, sd.V, -lightDir, sd.roughness);
    vec3 F = FresnelSchlick(max(dot(halfway, sd.V), 0.0), sd.F0); //roughness is handled by d and g.
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(sd.V, sd.N), 0.0) * max(dot(-lightDir, sd.N), 0.0) + 0.00001;
    vec3 fCookTorrence = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - sd.metallic; //kd theoretically either exists or is 0. (binary metal values)

    float NdotL = max(dot(sd.N, -lightDir),0.0);

    return (kD * fLambert + fCookTorrence) * radiance * NdotL;
}