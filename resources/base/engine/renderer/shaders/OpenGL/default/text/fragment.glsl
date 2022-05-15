#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler3D TexData;
uniform vec4 mcolor;
uniform vec4 bmcolor = vec4(0.0, 0.0, 0.0, 0.35);
uniform uint chr;

void main() {
    FragColor = mcolor * texture(TexData, vec3(TexCoord, (255.0 / 256.0) - (float(chr) / 256.0)));
    FragColor = mix(bmcolor, FragColor, FragColor.a);
}
