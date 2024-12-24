#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aDirection;
layout (location = 3) in int aTop;

out vec2 TexCoord;
out vec3 Normal;

uniform float texMultiplier;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

// Array of possible normals based on direction
const vec3 normals[] = vec3[](
vec3( 0,  0,  1), // 0
vec3( 0,  0, -1), // 1
vec3( 1,  0,  0), // 2
vec3(-1,  0,  0), // 3
vec3( 0,  1,  0), // 4
vec3( 0, -1,  0), // 5
vec3( 0, -1,  0)  // 6
);

const int aFrames = 32;
const float animationTime = 5.0;
const int texNum = 16;

void main()
{
	vec3 pos = aPos;
	
	if (aTop != 0)
	{
		pos.y -= 0.1;
		pos.y += (sin(pos.x * 1.5708 + time) + sin(pos.z * 1.5708 + time * 1.5)) * 0.05;
	}
	
	gl_Position = projection * view * model * vec4(pos, 1.0);
	
	float frame = mod(time / animationTime, 1.0) * aFrames;
	vec2 currentTex = aTexCoord;
	currentTex.x += mod(floor(frame), texNum);
	currentTex.y += floor(frame / texNum);
	TexCoord = currentTex * texMultiplier;
	
	int direction = clamp(aDirection, 0, 6);
	Normal = normalize(normals[direction]);
}
