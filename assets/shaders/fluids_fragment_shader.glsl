#version 420 core

in vec3 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in float vSkyLight;
in float vBlockLight;
in float Visibility;
in vec4 FragPosLightSpace;
in vec4 FragPosReflectionSpace;
layout(binding = 1) uniform sampler2D shadowMap;

out vec4 FragColor;

layout(binding = 0) uniform sampler2DArray tex;
layout(binding = 2) uniform sampler2D reflectionMap;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float ambientStrength;
uniform vec3 cameraPos;
uniform float time;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
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
	vec3 lightDir = normalize(-sunDirection);
	vec3 normal = normalize(Normal);
	float diff = max(dot(normal, lightDir), 0.0);
	
	float shadow = calculateShadow(FragPosLightSpace, normal, lightDir);
	vec3 diffuse = diff * sunColor * vSkyLight * (1.0 - shadow);

	vec3 ambient = vec3(ambientStrength);
	vec3 blockLight = vBlockLight * vec3(1.0, 0.85, 0.6);
	vec3 totalLight = ambient + max(diffuse, blockLight);
	totalLight = clamp(totalLight, 0.0, 1.4);

	vec4 texResult = texture(tex, TexCoord);
	
	if (texResult.a < 0.1)
		discard;
	
	// Reflection stuff
	
	// Figure out where this fragment is in the reflection camera's view.
    vec3 reflectionNDC = FragPosReflectionSpace.xyz / FragPosReflectionSpace.w;
    vec2 reflectionUV = reflectionNDC.xy * 0.5 + 0.5;
    
    // Wave distortion is added to make the water look more realistic.
    float wave1 = sin(FragPos.x * 0.8 + time * 0.7) * 0.003;
    float wave2 = sin(FragPos.z * 1.2 + time * 0.5) * 0.003;
    reflectionUV += vec2(wave1, wave2);
    
    reflectionUV = clamp(reflectionUV, 0.001, 0.999);
    
	vec3 reflectionColor = texture(reflectionMap, reflectionUV).rgb;
	
	// Adding a bit of blue tint to the reflection so it looks like water.
	vec3 waterTint = vec3(0.3, 0.5, 0.7);
	reflectionColor = mix(reflectionColor, reflectionColor * waterTint, 0.3);
	
	// A math trick to make the water reflect more when viewed from a distance.
	// This uses a simplified formula learned in class (Schlick's approximation).
	// Ref: https://en.wikipedia.org/wiki/Schlick%27s_approximation
	vec3 viewDir = normalize(cameraPos - FragPos);
	float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 2.0);
	fresnel = clamp(fresnel, 0.15, 0.65);  // Min 15%, max 65% reflection
	
	// Reduce the reflection at night when the sun is gone.
	float sunBrightness = length(sunColor);
	fresnel *= clamp(sunBrightness, 0.0, 1.0);
	
	// Putting it all together for the final color.
	vec3 waterColor = totalLight * texResult.rgb;
	
	if(texResult.a > 0.5 && fresnel > 0.01) {
        vec3 finalRGB = mix(waterColor, reflectionColor, fresnel);
        FragColor = vec4(finalRGB, texResult.a);
	} else {
	    FragColor = vec4(waterColor, texResult.a);
	}
}
