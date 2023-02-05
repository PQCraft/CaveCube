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

const mat3 rgb2yiq_mat = mat3(
    0.2989, 0.5959, 0.2115,
    0.5870, -0.2744, -0.5229,
    0.1140, -0.3216, 0.3114
);

vec3 rgb2yiq(vec3 c) {
    return rgb2yiq_mat * c;
}

const mat3 yiq2rgb_mat = mat3(
    1.0, 1.0, 1.0,
    0.956, -0.2720, -1.1060,
    0.6210, -0.6474, 1.7046
);

vec3 yiq2rgb(vec3 c) {
    return yiq2rgb_mat * c;
}

vec4 grabPixel(float x, float y, int smear) {
    vec4 tmp = texture(texData, vec2(x / fbsize.x, y / fbsize.y));
    for (int i = smear; i > 0; --i) {
        if ((x - float(i)) < 0.0) {
            tmp += vec4(0.0, 0.0, 0.0, 0.5);
        } else {
            tmp += texture(texData, vec2((x - float(i)) / fbsize.x, y / fbsize.y));
        }
    }
    tmp /= 1.0 + float(smear);
    /*
    if ((int(y) % 2) == 0) {
        tmp /= 1.1;
    }
    */
    return tmp;
}

void main() {
    vec2 texCoord2;
    if (fbtype == 0) {
        texCoord2.x = floor(texCoord.x * fbsize.x) / fbsize.x;
        texCoord2.y = floor(texCoord.y * fbsize.y) / fbsize.y;
    } else {
        texCoord2 = texCoord;
    }
    texCoord2.x *= fbsize.x;
    texCoord2.y *= fbsize.y;
    vec3 yiq1;
    vec4 tmp[2];
    tmp[0] = grabPixel(texCoord2.x, texCoord2.y, 2);
    tmp[1] = grabPixel(texCoord2.x + 7.0, texCoord2.y, 25);
    tmp[0].xyz = rgb2yiq(tmp[0].rgb);
    tmp[1].xyz = rgb2yiq(tmp[1].rgb);
    fragColor.a = tmp[0].a * 0.9 + tmp[1].a * 0.1;
    yiq1.x = tmp[0].x;
    yiq1.yz = tmp[1].yz;
    fragColor.rgb = yiq2rgb(yiq1);
    vec3 hsvFrag = rgb2hsv(fragColor.rgb);
    hsvFrag.x *= (1.0 + h);
    hsvFrag.y *= (1.0 + s);
    hsvFrag.z *= (1.0 + v);
    fragColor.rgb = hsv2rgb(hsvFrag) * mcolor;
    //float gamma = 1.5;
    //fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / gamma));
}
