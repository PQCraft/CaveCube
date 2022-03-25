#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;

uniform sampler2D TexData;
uniform vec3 viewPos;

uniform int fIs2D;

uniform int fIsText;
uniform vec3 textColor;

void main() {
    if (fIs2D == 0) {
        FragColor = (texture(TexData, TexCoord));
    } else {
        if (fIsText == 0) {
            FragColor = texture(TexData, TexCoord);
        } else {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(TexData, TexCoord).r);
            FragColor = vec4(textColor, 1.0) * sampled;
        }
    }
}
