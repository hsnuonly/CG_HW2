#version 430

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexColor;

out vec3 Color;

uniform mat4 um4p;
uniform mat4 um4v;
uniform mat4 um4n;

void main() {

	Color = VertexColor;

	gl_Position = um4n * vec4(VertexPosition, 1.0);
}
