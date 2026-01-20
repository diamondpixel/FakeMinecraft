#version 330 core

in vec3 TexCoord;
in vec3 Normal;
out vec4 FragColor;
uniform sampler2DArray tex;

vec3 ambient = vec3(0.5);
vec3 lightDirection = vec3(0.8, 1.0, 0.7);

void main()
{
	vec3 lightDir = normalize(-lightDirection);
	vec3 normal = normalize(Normal);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * vec3(1.0);
	vec3 lighting = ambient + diffuse;
	vec4 texResult = texture(tex, TexCoord);
	
	if (texResult.a < 0.1)
	discard;
	
	FragColor = vec4(lighting * texResult.rgb, texResult.a);
}
