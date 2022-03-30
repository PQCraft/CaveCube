#version 330
#pragma optimize(on)

in vec2 TexCoord;
in vec3 FragPos;
in float TexOff;

uniform sampler3D TexData;

void main() {
    vec4 color = texture(TexData, vec3(TexCoord, TexOff));
    if (color.a >= 0.5) {
        gl_FragColor = vec4(color.rgb, 1.0);
    } else {
        discard;
    }
}
