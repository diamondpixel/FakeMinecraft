#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aDirection;
layout (location = 3) in int aLayerIndex;
layout (location = 4) in int aLightLevel;
layout (location = 5) in int aTop;

out vec3 FragPos;
out vec3 Normal;
out vec3 TexCoord;
out float vSkyLight;
out float vBlockLight;
out float Visibility;
out vec4 FragPosLightSpace;
out vec4 FragPosReflectionSpace;


uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform mat4 lightSpaceMatrix;
uniform mat4 reflectionMatrix;

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

const int aFrames = 32;
const float animationTime = 5.0;

void main()
{
	vec3 pos = aPos;
	
	if (aTop != 0)
	{
		pos.y -= 0.1;
		pos.y += (sin(pos.x * 1.5708 + time) + sin(pos.z * 1.5708 + time * 1.5)) * 0.05;
	}
	
	gl_Position = projection * view * vec4(pos, 1.0);
	FragPos = pos;  // World-space position for reflection projection
	
	float frame = mod(time / animationTime, 1.0) * aFrames;
    int currentLayer = aLayerIndex + int(floor(frame));
    
	TexCoord = vec3(aTexCoord, float(currentLayer));
	
	int direction = clamp(aDirection, 0, 6);
	Normal = normalize(normals[direction]);
	FragPosLightSpace = lightSpaceMatrix * vec4(pos, 1.0);
	FragPosReflectionSpace = reflectionMatrix * vec4(pos, 1.0);
	
	int sky = (aLightLevel >> 4) & 0xF;
	int block = aLightLevel & 0xF;
	vSkyLight = float(sky) / 15.0;
	vBlockLight = float(block) / 15.0;
}