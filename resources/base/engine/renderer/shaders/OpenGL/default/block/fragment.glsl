#version 330
#pragma optimize(on)

in vec2 TexCoord;
in vec3 FragPos;
in float TexOff;

uniform sampler3D TexData;
uniform int dist;
uniform vec3 cam;

void main() {
    vec4 color = texture(TexData, vec3(TexCoord, TexOff));
    if (color.a >= 0.1) {
        gl_FragColor = vec4(color.rgba);
    } else {
        discard;
    }
    float mixv = clamp((distance(vec3(FragPos.x, 0, FragPos.z), vec3(cam.x, cam.z / 16, cam.z)) - float(dist) * 3) / (16 * float(dist) - float(dist) * 3), 0, 1);
    gl_FragColor = mix(gl_FragColor, vec4(0, 0.75, 1, gl_FragColor.a), mixv);
}
