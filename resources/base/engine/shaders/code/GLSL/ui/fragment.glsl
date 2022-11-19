in float texNum;
in vec4 mcolor;
in vec4 bmcolor;
in float chartw;
in float charth;
in float chartx;
in float charty;
uniform float scale;
flat in uint elemType;
flat in uint b;
flat in uint u;
flat in uint s;
uniform sampler2DArray fontTexData;

out vec4 fragColor;

void main() {
    vec2 texCoord = vec2(chartx / chartw, charty / charth);
    if (elemType == uint(1)) {
        if (bool(b)) {
            fragColor = max(texture(fontTexData, vec3(texCoord, texNum)), texture(fontTexData, vec3(vec2(texCoord.x - scale / chartw, texCoord.y), texNum)));
        } else {
            fragColor = texture(fontTexData, vec3(texCoord, texNum));
        }
        if (bool(u) && int(charty) >= int(charth) - scale) {
            fragColor = mcolor;
        }
        if (bool(s) && int(charty) >= int(charth / 2.0) - scale && int(charty) < int(charth / 2.0)) {
            fragColor = mcolor;
        }
        fragColor *= mcolor;
        fragColor = mix(bmcolor, fragColor, fragColor.a);
        if (fragColor.a == 0.0) discard;
    } else {
        fragColor = mcolor;
    }
}
