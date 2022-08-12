#version 330
#pragma optimize(on)

layout (location = 0) in vec4 data;

out vec2 texCoord;

uniform vec2 mpos;
uniform float xratio = 8.0 / 1024.0;
uniform float yratio = 16.0 / 768.0;
uniform float scale;

void main() {
    texCoord.x = data[2];
    texCoord.y = data[3];
    gl_Position = vec4((data[0] * xratio * scale) + mpos.x * 2 + xratio * scale - 1, 1 - (((1 - data[1]) * yratio * scale) + mpos.y * 2), 0, 1);
}
