#version 330
#pragma optimize(on)

out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec4 FragPos2;
in float TexOff;
in float light;
in float vAffine;

uniform sampler3D TexData;
uniform int dist;
uniform int vis;
uniform float vismul;
uniform vec3 cam;
uniform vec3 skycolor;
uniform vec3 mcolor;

void main() {
    if (FragPos2.z < 0) discard;
    vec4 color = texture(TexData, vec3(TexCoord / vAffine, TexOff));
    if (color.a > 0.0) {
        FragColor = vec4(color.rgba);
    } else {
        discard;
    }
    float mixv = clamp((distance(vec3(FragPos.x, FragPos.y, FragPos.z), vec3(cam.x, cam.y, cam.z)) - float(dist) * vismul * vis * 2) / (16 * float(dist) * vismul - float(dist) * vismul * vis * 2), 0, 1);
    FragColor.rgb *= light;
    FragColor.rgb *= mcolor;
    FragColor = mix(FragColor, vec4(skycolor, FragColor.a), mixv);
    float cl = 15;
    FragColor.rgb = round(FragColor.rgb * cl);
    FragColor.rgb /= cl;
}
