out vec4 fragColor;

in vec2 texCoord;

uniform sampler2DArray texData;
uniform vec4 mcolor;
uniform vec4 bmcolor;
uniform uint chr;

void main() {
    fragColor = mcolor * texture(texData, vec3(texCoord, float(chr)));
    fragColor = mix(bmcolor, fragColor, fragColor.a);
}
