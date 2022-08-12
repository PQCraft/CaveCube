#version 330
#pragma optimize(on)

out vec4 fragColor;

in vec2 texCoord;

uniform sampler2D texData;
uniform vec4 mcolor;

void main() {
    fragColor = mcolor * texture(texData, texCoord);
}
