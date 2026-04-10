#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "per_object_layout.glsl" //push constants

//set 0: per frame (camera)
#include "per_frame_layout.glsl" 

//set 1: shadowmap, dont need

//set 2: material data
layout(set = 2, binding = 0) uniform GLTFMaterialData
{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
	vec4 diffuse_transmission_factors;
	vec4 clearcoat_factors;
} materialData;
layout(set = 2, binding = 1) uniform sampler2D colorTex;
layout(set = 2, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 2, binding = 3) uniform sampler2D normalTex;
layout(set = 2, binding = 4) uniform sampler2D emissiveTex;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outFragPosWorld;
layout (location = 4) out vec4 outTangent;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);
	mat3 normalMatrix = transpose(inverse(mat3(PushConstants.render_matrix)));

	//pos
	outFragPosWorld = PushConstants.render_matrix * position;
	gl_Position 	= sceneData.viewproj * PushConstants.render_matrix * position;

	//normal/tangent
	outNormal 	   	= normalMatrix * v.normal;
	outTangent.xyz 	= normalMatrix * v.tangent.xyz;
	outTangent.w 	= v.tangent.w;

	//color
	outColor 		= v.color.xyz * materialData.colorFactors.xyz;	
	
	//uv
	outUV.x 		= v.uv_x;
	outUV.y 		= v.uv_y;
}