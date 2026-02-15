#version 420 core

in vec3 TexCoord;
in vec3 Normal;
in float vSkyLight;
in float vBlockLight;
// Shadow calculation variables
in vec4 FragPosLightSpace;
out vec4 FragColor;
layout(binding = 0) uniform sampler2DArray tex;
layout(binding = 1) uniform sampler2D shadowMap;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float ambientStrength;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // Perspective division
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if fragment is outside shadow map bounds - return 0.0 (no shadow)
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
        
    // Calculate bias to prevent shadow acne
    float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.0001);
    
    // Poisson disk sampling for smoother shadows (eliminates grid artifacts)
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
    );
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // Sample shadow map with Poisson disk pattern (2.5 texel radius)
    for(int i = 0; i < 16; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * 2.5;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
        shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
    }
    shadow /= 16.0;
    
    return shadow;
}

void main()
{
	// Directional sun light
	vec3 lightDir = normalize(-sunDirection); // SunDirection points FROM the sun. Negate to point TO sun.
	float diff = max(dot(Normal, lightDir), 0.0);
	
	// Calculate shadow
	float shadow = calculateShadow(FragPosLightSpace, Normal, lightDir);

	// Apply shadow to diffuse light
	vec3 diffuse = diff * sunColor * (1.0 - shadow);

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