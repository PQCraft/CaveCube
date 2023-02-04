in vec2 texCoord;
uniform sampler2D texData;
uniform vec3 mcolor;
uniform int fbtype;
uniform vec2 fbsize;

uniform float h;
uniform float s;
uniform float v;

out vec4 fragColor;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 texCoord2;
    if (fbtype == 0) {
        texCoord2.x = floor(texCoord.x * 320.0) / 320.0;
        texCoord2.y = floor(texCoord.y * 240.0) / 240.0;
    } else {
        texCoord2 = texCoord;
    }
    fragColor = texture(texData, texCoord2);
    float mult = 31.0;
    fragColor = floor(fragColor * mult);
    fragColor /= mult;
    vec3 hsvFrag = rgb2hsv(fragColor.rgb);
    hsvFrag.x *= (1.0 + h);
    hsvFrag.y *= (1.0 + s);
    hsvFrag.z *= (1.0 + v);
    fragColor.rgb = hsv2rgb(hsvFrag) * mcolor;
}
