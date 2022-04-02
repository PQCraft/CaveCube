#version 330
#pragma optimize(on)

layout (location = 0) in vec4 data;

out vec2 TexCoord;

void main() {
    TexCoord.x = data[2];
    TexCoord.y = data[3];
    gl_Position = vec4(data[0], data[1], 0, 1);
}
