in vec2 texCoord;
in float chr;
uniform sampler2DArray texData;
uniform vec4 mcolor;
uniform vec4 bmcolor;

out vec4 fragColor;

void main() {
    fragColor = mcolor * texture(texData, vec3(texCoord, chr));
    fragColor = mix(bmcolor, fragColor, fragColor.a);
}
