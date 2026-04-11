layout(set = 0, binding = 0) uniform SceneData
{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 camPos;
	mat4 lightViewProj;
} sceneData; //matches CPU side struct