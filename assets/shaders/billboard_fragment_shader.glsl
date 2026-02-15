#version 420 core

in vec3 Normal;
in vec3 FragPos;
in vec3 TexCoord;
in float vSkyLight;
in float vBlockLight;
in float Visibility;
in vec4 FragPosLightSpace;
layout(binding = 1) uniform sampler2D shadowMap;

out vec4 FragColor;

layout(binding = 0) uniform sampler2DArray tex;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float ambientStrength;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
    // Bias adjustment for large Z-range
    float bias = max(0.00005 * (1.0 - dot(normal, lightDir)), 0.00001);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{
	vec4 texResult = texture(tex, TexCoord); 
	if(texResult.a < 0.1)
		discard;

	vec3 ambient = ambientStrength * sunColor;
	vec3 norm = normalize(Normal);
	// Invert sun direction for lightDir? Assuming consistent with world shader
	vec3 lightDir = normalize(-sunDirection); 
	float diff = max(dot(norm, lightDir), 0.0);
	
	// Shadows disabled on billboards (grass/trees) - visually distracting
	float shadow = 0.0; // calculateShadow(FragPosLightSpace, norm, lightDir);
	vec3 diffuse = diff * sunColor * (1.0 - shadow);
	
	vec3 result = (ambient + diffuse) * texResult.rgb;
	FragColor = vec4(result, texResult.a);
}