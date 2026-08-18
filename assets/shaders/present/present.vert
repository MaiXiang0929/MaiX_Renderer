#version 330 core

// 方形平面只需要位置与纹理坐标；法线属性由通用 Mesh 布局保留但不参与计算。
layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 texCoord;

out vec2 fragTexCoord;

void main()
{
    // Present quad already uses clip-space positions; scene navigation belongs to the view camera.
    gl_Position = vec4(pos.xy, 0.0, 1.0);
    fragTexCoord = texCoord;
}
