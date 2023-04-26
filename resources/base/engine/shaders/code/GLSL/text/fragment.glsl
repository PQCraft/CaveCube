in vec3 texCoord;
in vec4 fgc;
in vec4 bgc;
flat in uint b;
flat in uint u;
flat in uint s;
uniform sampler2DArray texData;
uniform vec2 texsize;

out vec4 fragColor;

void main() {
    fragColor = texture(texData, texCoord);
    vec4 bFragColor = texture(texData, vec3(texCoord.x + 1.0 / texsize.x, texCoord.yz));
    if (bool(b)) {
        fragColor = max(fragColor, bFragColor);
    }
    float tcy = texCoord.y * texsize.y;
    if (bool(u)) {
        if (tcy < 1.0) {
            fragColor = vec4(1.0);
        }
    }
    if (bool(s)) {
        float tmp = floor(texsize.y / 2.0);
        if (tcy >= tmp && tcy < tmp + 1.0) {
            fragColor = vec4(1.0);
        }
    }
    fragColor *= mix(bgc, fgc, fragColor.a);
}
