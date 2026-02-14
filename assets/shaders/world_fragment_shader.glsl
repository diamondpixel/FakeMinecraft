#version 330 core

in vec3 TexCoord;
in vec3 Normal;
in float vSkyLight;
in float vBlockLight;
out vec4 FragColor;
uniform sampler2DArray tex;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float ambientStrength;

void main()
{
	// Directional sun light
	vec3 lightDir = normalize(-sunDirection);
	float diff = max(dot(Normal, lightDir), 0.0);
	vec3 diffuse = diff * sunColor * vSkyLight;

	// Ambient (sky contribution)
	vec3 ambient = vec3(ambientStrength);

	// Block light (lava etc) - warm orange glow
	vec3 blockLight = vBlockLight * vec3(1.0, 0.85, 0.6);

	// Combine: max of (sun light, block light) + ambient
	vec3 totalLight = ambient + max(diffuse, blockLight);
	totalLight = clamp(totalLight, 0.0, 1.4);

	vec4 texResult = texture(tex, TexCoord);
	if (texResult.a == 0)
	    discard;
	FragColor = vec4(texResult.rgb * totalLight, 1.0);
}