#ifndef PBR_COMMON_GLSL
#define PBR_COMMON_GLSL

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    const float PI = 3.14159265359;

    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness); //geometry obstruction
    float ggx1  = GeometrySchlickGGX(NdotL, roughness); //geometry shadowing

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    vec3 F90 = max(vec3(1.0 - roughness), F0);
    return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// FILAMENT
float NormalFiltering(float perceptualRoughness, const vec3 worldNormal) {
    const float specularAAVariance = 0.15; 
    const float specularAAThreshold = 0.2;
    // Kaplanyan 2016, "Stable specular highlights"
    // Tokuyoshi 2017, "Error Reduction and Simplification for Shading Anti-Aliasing"
    // Tokuyoshi and Kaplanyan 2019, "Improved Geometric Specular Antialiasing"
    // This implementation is meant for deferred rendering in the original paper but
    // we use it in forward rendering as well (as discussed in Tokuyoshi and Kaplanyan
    // 2019). The main reason is that the forward version requires an expensive transform
    // of the half vector by the tangent frame for every light. This is therefore an
    // approximation but it works well enough for our needs and provides an improvement
    // over our original implementation based on Vlachos 2015, "Advanced VR Rendering".
    vec3 du = dFdx(worldNormal);
    vec3 dv = dFdy(worldNormal);
    float variance = specularAAVariance * (dot(du, du) + dot(dv, dv));
    float alpha = perceptualRoughness * perceptualRoughness;
    float kernelRoughness = min(2.0 * variance, specularAAThreshold);
    float squareAlpha = clamp(alpha * alpha + kernelRoughness, 0.0, 1.0);
    return sqrt(sqrt(squareAlpha));
}

#endif // PBR_COMMON_GLSL