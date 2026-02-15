#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in int aLayerIndex;
layout (location = 4) in int aLightLevel;

out vec3 FragPos;
out vec3 TexCoord;
out vec3 Normal;
out float vSkyLight;
out float vBlockLight;
out vec4 FragPosLightSpace;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
	gl_Position = projection * view * vec4(aPos, 1.0);
	FragPos = vec3(vec4(aPos.x, aPos.y, aPos.z, 1.0));
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
	TexCoord = vec3(aTexCoord, float(aLayerIndex));
	Normal = vec3(0.0, 1.0, 0.0); // Billboard uses upward normal for lighting
	
	// Unpack light levels (0-15)
	int sky = (aLightLevel >> 4) & 0xF;
	int block = aLightLevel & 0xF;
	
	vSkyLight = float(sky) / 15.0;
	vBlockLight = float(block) / 15.0;
}