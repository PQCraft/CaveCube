noperspective in vec2 texCoord;
in vec3 fragPos;
in float texOffset;
in vec3 light;
uniform vec3 skycolor;
uniform sampler2DArray texData;
uniform int dist;
uniform float fogNear;
uniform float fogFar;
uniform vec3 cam;

out vec4 fragColor;

void main() {
    vec4 color = texture(texData, vec3(texCoord, texOffset));
    if (color.a > 0.0) {
        fragColor.r = float(color.r * 8.0) / 8.0;
        fragColor.g = float(color.g * 8.0) / 8.0;
        fragColor.b = float(color.b * 4.0) / 4.0;
        fragColor.a = color.a;
    } else {
        discard;
    }
    float fogdist = distance(vec3(fragPos.x, abs(cam.y - fragPos.y) * 0.5, fragPos.z), vec3(cam.x, 0, cam.z));
    float fogdmin = (float(dist) * 16.0) * fogNear;
    float fogdmax = (float(dist) * 16.0) * fogFar;
    fragColor.rgb *= light;
    fragColor = mix(fragColor, vec4(skycolor, fragColor.a), 1.0 - clamp((fogdmax - fogdist) / (fogdmax - fogdmin), 0.0, 1.0));
}
