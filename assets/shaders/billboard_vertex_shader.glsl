#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in int aLayerIndex;
layout (location = 4) in int aLightLevel;

out vec3 TexCoord;
out float vSkyLight;
out float vBlockLight;


uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * vec4(aPos, 1.0);
	TexCoord = vec3(aTexCoord, float(aLayerIndex));
	
	// Unpack light levels (0-15)
	int sky = (aLightLevel >> 4) & 0xF;
	int block = aLightLevel & 0xF;
	
	vSkyLight = float(sky) / 15.0;
	vBlockLight = float(block) / 15.0;
}