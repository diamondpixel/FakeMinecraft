#version 330 core

in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D skyTex;
uniform float brightness;

void main()
{
    vec4 texResult = texture(skyTex, TexCoord);
    if (texResult.a < 0.01)
        discard;
    FragColor = vec4(texResult.rgb * brightness, texResult.a);
}
