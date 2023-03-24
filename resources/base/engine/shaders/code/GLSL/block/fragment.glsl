in vec2 texCoord;
#ifndef SORTTRANSPARENT
flat in uint transparency;
#endif
in vec3 fragPos;
in float texOffset;
in vec3 light;
uniform vec3 skycolor;
uniform sampler2DArray texData;
uniform int dist;
uniform float fogNear;
uniform float fogFar;
uniform vec3 cam;
uniform bool mipmap;

out vec4 fragColor;

void main() {
    #ifndef SORTTRANSPARENT
    vec2 dx, dy;
    if (mipmap && transparency != uint(1)) {
        dx = dFdx(texCoord);
        dy = dFdy(texCoord);
    } else {
        dx = vec2(0.0);
        dy = vec2(0.0);
    }
    fragColor = textureGrad(texData, vec3(texCoord, texOffset), dx, dy);
    if (transparency == uint(1) && fragColor.a == 0.0) {
        discard;
        return;
    }
    #else
    fragColor = texture(texData, vec3(texCoord, texOffset));
    #endif
    float fogdist = distance(vec3(fragPos.x, abs(cam.y - fragPos.y) * 0.67, fragPos.z), vec3(cam.x, 0, cam.z));
    float fogdmin = (float(dist) * 16.0) * fogNear;
    float fogdmax = (float(dist) * 16.0) * fogFar;
    fragColor.rgb *= light;
    fragColor = mix(vec4(skycolor, fragColor.a), fragColor, clamp((fogdmax - fogdist) / (fogdmax - fogdmin), 0.0, 1.0));
}
