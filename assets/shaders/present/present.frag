#version 330 core

in vec2 fragTexCoord;

layout(location = 0) out vec4 color;

uniform sampler2D renderedTexture;
uniform sampler2D editorOverlayTexture;

void main()
{
    vec3 sceneColor = texture(renderedTexture, fragTexCoord).rgb;
    vec4 overlay = texture(editorOverlayTexture, fragTexCoord);
    color = vec4(
        overlay.rgb + sceneColor * (1.0 - overlay.a),
        1.0);
}
