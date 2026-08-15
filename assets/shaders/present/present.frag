#version 330 core

in vec2 fragTexCoord;

layout(location = 0) out vec4 color;

uniform sampler2D renderedTexture;

void main()
{
    color = texture(renderedTexture, fragTexCoord);
}
