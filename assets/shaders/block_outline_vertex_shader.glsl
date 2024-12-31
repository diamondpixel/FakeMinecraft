#version 330 core

layout (location = 0) in vec3 aPos;

uniform vec4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Use only the xyz part of the vec4 (model.xyz) and ignore the w component
    gl_Position = projection * view * vec4(aPos + model.xyz, 1.0);
}
