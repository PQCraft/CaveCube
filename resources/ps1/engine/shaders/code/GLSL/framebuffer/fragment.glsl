in vec2 texCoord;
uniform sampler2D texData;
uniform vec4 mcolor;

out vec4 fragColor;

void main() {
    vec2 texCoord2;
    texCoord2.x = floor(texCoord.x * 384.0) / 384.0;
    texCoord2.y = floor(texCoord.y * 240.0) / 240.0;
    fragColor = mcolor * texture(texData, texCoord2);
    float mult = 31;
    fragColor = floor(fragColor * mult);
    fragColor /= mult;
}
