layout(set = 3, binding = 0) uniform RenderOptions
{   
	uint enableSpecularAA;
	uint enableShadows;
	float iblIntensity;
	float prefilterMaxLod;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
} renderOptions; //matches CPU side struct