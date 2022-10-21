in vec2 texCoord;
in float chr;
in vec4 mcolor;
in vec4 bmcolor;
in uint texIndex;
uniform sampler2DArray texData[2];

out vec4 fragColor;

void main() {
    fragColor = mcolor * texture(texData[texIndex], vec3(texCoord, chr));
    fragColor = mix(bmcolor, fragColor, fragColor.a);
}
