#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec4 FragPos2;
in float TexOff;

uniform sampler3D TexData;
uniform int dist;
uniform vec3 cam;
uniform vec3 skycolor;
uniform vec3 mcolor;

void main() {
    if (FragPos2.z < 0) discard;
    if (FragPos2.z > dist * 16) discard;
    vec4 color = texture(TexData, vec3(TexCoord, TexOff));
    if (color.a >= 0.1) {
        FragColor = vec4(color.rgba);
    } else {
        discard;
    }
    float mixv = clamp((distance(vec3(FragPos.x, 0, FragPos.z), vec3(cam.x, cam.z / 16, cam.z)) - float(dist) * 3) / (16 * float(dist) - float(dist) * 3), 0, 1);
    FragColor.rgb *= mcolor;
    FragColor = mix(FragColor, vec4(skycolor, FragColor.a), mixv);
}
