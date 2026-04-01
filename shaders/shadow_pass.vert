#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

//set 0: per frame
#include "per_frame_layout.glsl"
// push constants
#include "per_object_layout.glsl"

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPos = PushConstants.render_matrix * vec4(v.position, 1.0);
    gl_Position = sceneData.lightViewProj * worldPos;
}