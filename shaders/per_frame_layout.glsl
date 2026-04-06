layout(set = 0, binding = 0) uniform SceneData
{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	float iblIntensity;
	float prefilterMaxLod;
	float _padding0;
	float _padding1;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 camPos;
	mat4 lightViewProj;
} sceneData; //matches CPU side struct