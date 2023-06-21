in vec2 texCoord;
in vec4 color;
uniform sampler2D texData;

out vec4 fragColor;

void main() {
    fragColor = texture(texData, texCoord) * color;
}
