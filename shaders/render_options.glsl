layout(set = 3, binding = 0) uniform RenderOptions
{   
	uint enableSpecularAA;
	float iblIntensity;
	float prefilterMaxLod;
	float _paddingRenderOptions0;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
} renderOptions; //matches CPU side struct