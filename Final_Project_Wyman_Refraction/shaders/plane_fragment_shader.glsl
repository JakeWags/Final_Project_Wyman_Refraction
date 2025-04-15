#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;

uniform sampler2D background;

void main()
{
    // Sample the cubemap using the combined direction
    vec4 envColor = texture(background, TexCoords);

    FragColor = envColor;
}