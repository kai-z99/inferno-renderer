#ifndef SURFACE_DATA_GLSL
#define SURFACE_DATA_GLSL

struct SurfaceData
{
    vec3 P; //world space point
    vec3 N; //normal
    vec3 V; //frag to cam

    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 emissive;

    //KHR_materials_diffuse_transmission
    vec3 diffuseTransmissionColor;
    float diffuseTransmissionFactor;

    //KHR_materials_clearcoat
    float clearcoatFactor;
    float clearcoatRoughness;
    vec3 clearcoatNormal;

    vec3 F0;
    float NdotV;
    vec3 R; //reflect(-V, N)
};

#endif // SURFACE_DATA_GLSL