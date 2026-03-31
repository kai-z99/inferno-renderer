#version 450


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D hdrImage;

layout( push_constant ) uniform constants
{
	int tonemap_index;
} PushConstants;

vec3 reinhard(vec3 rgb)
{
    return rgb / (vec3(1.0) + rgb);
}

vec3 aces(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float uchimura(float x, float P, float a, float m, float l, float c, float b)
{
    float l0 = ((P - m) * l) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = step(m + l0, x);
    float w1 = 1.0 - w0 - w2;

    float T = m * pow(x / m, c) + b;
    float L = m + a * (x - m);
    float S = P - (P - S1) * exp(CP * (x - S0));

    return T * w0 + L * w1 + S * w2;
}

vec3 uchimura(vec3 color)
{
    const float P = 1.0;
    const float a = 1.0;
    const float m = 0.22;
    const float l = 0.4;
    const float c = 1.33;
    const float b = 0.0;

    return vec3
    (
        uchimura(color.r, P, a, m, l, c, b),
        uchimura(color.g, P, a, m, l, c, b),
        uchimura(color.b, P, a, m, l, c, b)
    );
}

void main() 
{
    vec3 col = texture(hdrImage, inUV).xyz;
    switch (PushConstants.tonemap_index)
    {
        case 0:
            col = aces(col); 
            break;
        case 1:
            col = reinhard(col);
            break; 
        default:
            col = uchimura(col);
            break;
    }

	outFragColor = vec4(col, 1.0);
}