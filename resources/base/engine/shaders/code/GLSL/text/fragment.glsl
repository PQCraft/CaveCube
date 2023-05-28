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
    float bx = texCoord.x + 1.0 / texsize.x;
    vec4 bFragColor = texture(texData, vec3(bx, texCoord.yz));
    #ifdef USEGLES
    if (texCoord.x > 1.0 || texCoord.x < 0.0) {
        fragColor = vec4(0.0);
    }
    if (texCoord.y > 1.0 || texCoord.y < 0.0) {
        fragColor = vec4(0.0);
        bFragColor = vec4(0.0);
    }
    if (bx > 1.0 || bx < 0.0) {
        bFragColor = vec4(0.0);
    }
    #endif
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
