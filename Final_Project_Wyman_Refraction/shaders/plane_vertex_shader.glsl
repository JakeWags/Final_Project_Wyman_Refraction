#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 FragPos;

uniform mat4 mvp;

void main()
{
    TexCoords = aTexCoords;
    FragPos = vec3(mvp * vec4(aPos, 1.0));
    gl_Position = mvp * vec4(aPos, 1.0);
}