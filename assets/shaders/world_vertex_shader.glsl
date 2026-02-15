#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aDirection;
layout (location = 3) in int aLayerIndex;
layout (location = 4) in int aLightLevel;

out vec3 FragPos;
out vec3 Normal;
out vec3 TexCoord;
out float vSkyLight;
out float vBlockLight;
out float Visibility;
out vec4 FragPosLightSpace;


uniform mat4 view;
uniform mat4 projection;
uniform vec3 sunColor;
uniform float ambientStrength;
uniform mat4 lightSpaceMatrix;

// Array of possible normals based on direction
const vec3 normals[] = vec3[](
vec3( 0,  0, -1), // 0 NORTH
vec3( 0,  0,  1), // 1 SOUTH
vec3(-1,  0,  0), // 2 WEST
vec3( 1,  0,  0), // 3 EAST
vec3( 0, -1,  0), // 4 BOTTOM
vec3( 0,  1,  0), // 5 TOP
vec3( 0,  0,  0)  // 6 PLACEHOLDER
);
void main()
{
	gl_Position = projection * view * vec4(aPos, 1.0);
	FragPos = aPos;
	TexCoord = vec3(aTexCoord, float(aLayerIndex));
	Normal = normals[aDirection];
	FragPosLightSpace = lightSpaceMatrix * vec4(aPos, 1.0);
	
	// Unpack light levels (0-15)
	int sky = (aLightLevel >> 4) & 0xF;
	int block = aLightLevel & 0xF;
	
	vSkyLight = float(sky) / 15.0;
	vBlockLight = float(block) / 15.0;
}