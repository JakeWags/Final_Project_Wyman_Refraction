#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 mv;
uniform mat3 mvNormal;
uniform mat4 p;

out vec3 Normal;
out vec3 FragPos;
out vec2 TexCoord;

void main()
{
    Normal = mvNormal * aNormal;
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoord = aTexCoords;

    gl_Position = p * mv * vec4(aPos, 1.0);
}
