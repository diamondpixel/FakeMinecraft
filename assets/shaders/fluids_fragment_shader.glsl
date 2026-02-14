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
	vec3 lightDir = normalize(-sunDirection);
	vec3 normal = normalize(Normal);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * sunColor * vSkyLight;

	vec3 ambient = vec3(ambientStrength);
	vec3 blockLight = vBlockLight * vec3(1.0, 0.85, 0.6);
	vec3 totalLight = ambient + max(diffuse, blockLight);
	totalLight = clamp(totalLight, 0.0, 1.4);

	vec4 texResult = texture(tex, TexCoord);
	
	if (texResult.a < 0.1)
	discard;
	
	FragColor = vec4(totalLight * texResult.rgb, texResult.a);
}
