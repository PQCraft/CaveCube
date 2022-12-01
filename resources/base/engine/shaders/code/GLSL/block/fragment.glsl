in vec2 texCoord;
in vec3 fragPos;
in float texOffset;
in vec3 light;
uniform sampler2DArray texData;
uniform int dist;
uniform float fogNear;
uniform float fogFar;
uniform vec3 cam;
uniform vec3 skycolor;

out vec4 fragColor;

void main() {
    vec4 color = texture(texData, vec3(texCoord, texOffset));
    if (color.a > 0.0) {
        fragColor = vec4(color.rgba);
    } else {
        discard;
    }
    float fogdist = distance(fragPos.xz, cam.xz);
    float fogdmin = (float(dist) * 16.0) * fogNear;
    float fogdmax = (float(dist) * 16.0) * fogFar;
    fragColor.rgb *= light;
    fragColor = mix(fragColor, vec4(skycolor, fragColor.a), 1.0 - clamp((fogdmax - fogdist) / (fogdmax - fogdmin), 0.0, 1.0));
}
