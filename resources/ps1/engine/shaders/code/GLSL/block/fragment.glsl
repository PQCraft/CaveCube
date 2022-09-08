noperspective in vec2 texCoord;
in vec3 fragPos;
in float texOffset;
in vec3 light;
uniform sampler2DArray texData;
uniform int dist;
uniform int vis;
uniform float vismul;
uniform vec3 cam;
uniform vec3 skycolor;
uniform vec3 mcolor;

out vec4 fragColor;

void main() {
    vec4 color = texture(texData, vec3(texCoord, texOffset));
    if (color.a > 0.0) {
        fragColor = vec4(color.rgba);
    } else {
        discard;
    }
    float mixv = clamp((distance(vec3(fragPos.x, fragPos.y, fragPos.z), vec3(cam.x, cam.y, cam.z)) - float(dist) * vismul * float(vis) * 2.0) / (16.0 * float(dist) * vismul - float(dist) * vismul * float(vis) * 2.0), 0.0, 1.0);
    fragColor.rgb *= light;
    fragColor.rgb *= mcolor;
    fragColor = mix(fragColor, vec4(skycolor, fragColor.a), mixv);
    float mult = 31;
    fragColor = floor(fragColor * mult);
    fragColor /= mult;
}
