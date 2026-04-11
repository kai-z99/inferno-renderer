#ifndef PBR_COMMON_GLSL
#define PBR_COMMON_GLSL

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
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

float V_Kelemen(float LoH) 
{
    return 0.25 / (LoH * LoH);
}

//Assumes F90 = vec3(1.0)
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
float NormalFiltering(float perceptualRoughness, const vec3 worldNormal) 
{
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





vec3 getIORTfromAirToSurfaceR0(vec3 f0) 
{
    vec3 sqrtF0 = sqrt(f0);
    return (1. + sqrtF0) / (1. - sqrtF0);
}

// Conversion FO/IOR
vec3 getR0fromIORs(vec3 iorT, float iorI)
{
    return ((iorT - vec3(iorI)) / (iorT + vec3(iorI))) * ((iorT - vec3(iorI)) / (iorT + vec3(iorI)));
}

float getR0fromIORs(float iorT, float iorI) 
{
    return ((iorT - iorI) / (iorT + iorI)) * ((iorT - iorI) / (iorT + iorI));
}

// Fresnel equations for dielectric/dielectric interfaces.
// Ref: https://belcour.github.io/blog/research/publication/2017/05/01/brdf-thin-film.html
// Evaluation XYZ sensitivity curves in Fourier space
vec3 EvalSensitivity(float opd, vec3 shift) 
{
    const mat3 XYZ_TO_REC709 = mat3(
     3.2404542, -0.9692660,  0.0556434,
    -1.5371385,  1.8760108, -0.2040259,
    -0.4985314,  0.0415560,  1.0572252
    );

    float phase = 2.0 * PI * opd * 1.0e-9;

    const vec3 val = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
    const vec3 pos = vec3(1.6810e+06, 1.7953e+06, 2.2084e+06);
    const vec3 var = vec3(4.3278e+09, 9.3046e+09, 6.6121e+09);

    vec3 xyz = val * sqrt(2.0 * PI * var) * cos(pos * phase + shift) * exp(-(phase * phase) * var);
    xyz.x += 9.7470e-14 * sqrt(2.0 * PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift[0]) * exp(-4.5282e+09 * (phase*phase));
    xyz /= 1.0685e-7;

    vec3 srgb = XYZ_TO_REC709 * xyz;
    return srgb;
}

//BabylonJS's implemenatation of
//Belcour, L. and Barla, P. (2017): A Practical Extension to Microfacet Theory for the Modeling of Varying Iridescence
vec3 FresnelIridescence(float outsideIOR, float eta2, float cosTheta1, float thinFilmThickness, vec3 baseF0) 
{
    vec3 I = vec3(1.0);

    // Force iridescenceIOR -> outsideIOR when thinFilmThickness -> 0.0
    float iridescenceIOR = mix(outsideIOR, eta2, smoothstep(0.0, 0.03, thinFilmThickness));
    // Evaluate the cosTheta on the base layer (Snell law)
    float sinTheta2Sq = ((outsideIOR / iridescenceIOR) * (outsideIOR / iridescenceIOR)) * (1.0 - (cosTheta1 * cosTheta1));

    // Handle TIR:
    float cosTheta2Sq = 1.0 - sinTheta2Sq;
    if (cosTheta2Sq < 0.0) 
    {
        return I;
    }

    float cosTheta2 = sqrt(cosTheta2Sq);

    // First interface
    float R0 = getR0fromIORs(iridescenceIOR, outsideIOR);
    float R12 = FresnelSchlick(cosTheta1, vec3(R0)).r; //F90 = 1
    float R21 = R12;
    float T121 = 1.0 - R12;
    float phi12 = 0.0;
    if (iridescenceIOR < outsideIOR) phi12 = PI;
    float phi21 = PI - phi12;

    // Second interface
    vec3 baseIOR = getIORTfromAirToSurfaceR0(clamp(baseF0, 0.0, 0.9999)); // guard against 1.0
    vec3 R1 = getR0fromIORs(baseIOR, iridescenceIOR); //F90 = 1
    vec3 R23 = FresnelSchlick(cosTheta2, R1);
    vec3 phi23 = vec3(0.0);
    if (baseIOR[0] < iridescenceIOR) phi23[0] = PI;
    if (baseIOR[1] < iridescenceIOR) phi23[1] = PI;
    if (baseIOR[2] < iridescenceIOR) phi23[2] = PI;

    // Phase shift
    float opd = 2.0 * iridescenceIOR * thinFilmThickness * cosTheta2;
    vec3 phi = vec3(phi21) + phi23;

    // Compound terms
    vec3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
    vec3 r123 = sqrt(R123);
    vec3 Rs = (T121*T121) * R23 / (vec3(1.0) - R123);

    // Reflectance term for m = 0 (DC term amplitude)
    vec3 C0 = R12 + Rs;
    I = C0;

    // Reflectance term for m > 0 (pairs of diracs)
    vec3 Cm = Rs - T121;
    for (int m = 1; m <= 2; ++m)
    {
        Cm *= r123;
        vec3 Sm = 2.0 * EvalSensitivity(float(m) * opd, float(m) * phi);
        I += Cm * Sm;
    }

    // Since out of gamut colors might be produced, negative color values are clamped to 0.
    return max(I, vec3(0.0));
}





#endif // PBR_COMMON_GLSL