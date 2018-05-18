#version 430

in vec3 Color;

out vec4 FragColor;

struct LightInfo{
	vec4 Position;	// Light position in eye coords
	vec4 La;		// Ambient light intensity
	vec4 Ld;		// Diffuse light intensity
	vec4 Ls;		// Specular light intensity
};

uniform LightInfo light[3];

void main() {
	FragColor = vec4(1.f,1.f,1.f,1.f);
}
