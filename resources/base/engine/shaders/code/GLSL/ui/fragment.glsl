in vec2 texCoord;
in float texNum;
in vec4 mcolor;
in vec4 bmcolor;
flat in uint texIndex;
uniform sampler2DArray texData[2];

out vec4 fragColor;

void main() {
    if (bool(texIndex)) {
        fragColor = mcolor * texture(texData[1], vec3(texCoord, texNum));
    } else {
        fragColor = mcolor * texture(texData[0], vec3(texCoord, texNum));
    }
    fragColor = mix(bmcolor, fragColor, fragColor.a);
}
