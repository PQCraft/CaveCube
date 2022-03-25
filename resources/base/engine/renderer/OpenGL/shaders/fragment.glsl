#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;

uniform sampler2D TexData;

void main() {
    FragColor = texture(TexData, TexCoord);
    FragColor.a = 1.0;
}
