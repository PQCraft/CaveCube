#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D TexData;
uniform vec4 mcolor;

void main() {
    FragColor = mcolor * texture(TexData, TexCoord);
}
