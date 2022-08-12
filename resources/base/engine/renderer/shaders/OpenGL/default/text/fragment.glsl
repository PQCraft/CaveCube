#version 330
#pragma optimize(on)

out vec4 fragColor;

in vec2 texCoord;

uniform sampler3D texData;
uniform vec4 mcolor;
uniform vec4 bmcolor = vec4(0.0, 0.0, 0.0, 0.35);
uniform uint chr;

void main() {
    fragColor = mcolor * texture(texData, vec3(texCoord, (255.0 / 256.0) - (float(chr) / 256.0)));
    fragColor = mix(bmcolor, fragColor, fragColor.a);
}
