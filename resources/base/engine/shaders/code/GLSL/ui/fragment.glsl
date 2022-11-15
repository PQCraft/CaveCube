in vec2 texCoord;
in float texNum;
in vec4 mcolor;
in vec4 bmcolor;
flat in uint elemType;
uniform sampler2DArray fontTexData;

out vec4 fragColor;

void main() {
    if (elemType == uint(1)) {
        fragColor = mcolor * texture(fontTexData, vec3(texCoord, texNum));
        fragColor = mix(bmcolor, fragColor, fragColor.a);
        if (fragColor.a == 0.0) discard;
    } else {
        fragColor = mcolor;
    }
}
