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
            break;
    }

	outFragColor = vec4(col, 1.0);
}