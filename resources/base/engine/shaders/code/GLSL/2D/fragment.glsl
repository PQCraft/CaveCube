in vec2 texCoord;
uniform sampler2D texData;
uniform vec4 mcolor;

out vec4 fragColor;

void main() {
    fragColor = mcolor * texture(texData, texCoord);
}
