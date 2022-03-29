#version 330
#pragma optimize(on)

in vec2 TexCoord;
in vec3 FragPos;

uniform sampler3D TexData;
uniform sampler2D TexData2D;

uniform int notBlock = 0;
uniform int blockID = 0;

void main() {
    if (notBlock == 0) {
        vec4 color = texture(TexData, vec3(TexCoord, float(blockID) / 256.0));
        if (color.a >= 0.5) {
            gl_FragColor = vec4(color.rgb, 1.0);
        } else {
            discard;
        }
    } else {
        gl_FragColor = texture(TexData2D, TexCoord);
    }
}
