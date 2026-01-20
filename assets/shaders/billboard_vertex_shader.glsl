#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in int aLayerIndex;

out vec3 TexCoord;
out vec3 Normal;
uniform float texMultiplier;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * vec4(aPos, 1.0);
	TexCoord = vec3(aTexCoord, float(aLayerIndex)); // local UV + layer
}