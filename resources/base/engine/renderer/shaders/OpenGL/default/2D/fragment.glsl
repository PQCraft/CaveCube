#version 330
#pragma optimize(on)

in vec2 TexCoord;

uniform sampler2D TexData;

void main() {
    gl_FragColor = texture(TexData, TexCoord);
    gl_FragColor.a = gl_FragColor.a / 10;
}
