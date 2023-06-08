//layout (location = 0) in uint data[0];
//// [8 bits: X][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1][13 bits: Y][8 bits: Z]
//layout (location = 1) in uint data[1];
//// [index in info UBO]
//layout (std140) uniform bdata {
//    uint data[3145728];
//};
//// [8 bits: texture X][8 bits: texture Y][2 bits: transparency][2 bits: texture X, Y + 1][4 bits: AO texture][5 bits: 0][3 bits: face ID]
//// [16 bits: texture info index][4 bits: top bit of (0, 0) R, G, B, N][4 bits: top bit of (1, 0) R, G, B, N][4 bits: top bit of (0, 1) R, G, B, N][4 bits: top bit of (1, 1) R, G, B, N]
//// [16 bits: (0, 0) R, G, B, N & 0xF][16 bits: (1, 0) R, G, B, N & 0xF]
//// [16 bits: (0, 1) R, G, B, N & 0xF][16 bits: (1, 1) R, G, B, N & 0xF]
//layout (std140) uniform tinfo {
//    uint info[98304];
//};
//// [16 bits: texture offset][8 bits: animation mod][8 bits: animation divisor]

layout (location = 0) in uvec4 data;
// [8 bits: X][16 bits: Y][8 bits: Z]
// [3 bits: reserved][5 bits: R][3 bits: reserved][5 bits: G][3 bits: reserved][5 bits: B][3 bits: reserved][5 bits: natural]
// [16 bits: texture offset][8 bits: animation mod][8 bits: animation divisor]
// [8 bits: texture X][8 bits: texture Y][6 bits: reserved][2 bits: transparency][3 bits: reserved][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1][1 bit: texture X + 1][1 bit: texture Y + 1]
uniform mat4 view;
uniform mat4 projection;
uniform vec2 ccoord;
uniform uint aniMult;
uniform vec3 natLight;
uniform vec3 skycolor;

out vec2 texCoord;
#ifndef OPT_SORTTRANSPARENT
flat out uint transparency;
#endif
out vec3 fragPos;
out float texOffset;
out vec3 light;

void main() {
    fragPos.x = (float(((data[0] >> 24) & uint(255)) + ((data[3] >> 4) & uint(1)))) / 16.0 - 8.0;
    fragPos.y = (float(((data[0] >> 8) & uint(65535)) + ((data[3] >> 3) & uint(1)))) / 16.0;
    fragPos.z = ((float((data[0] & uint(255)) + ((data[3] >> 2) & uint(1)))) / 16.0 - 8.0) * -1.0;
    texCoord.x = (float(((data[3] >> 24) & uint(255)) + ((data[3] >> 1) & uint(1)))) / 16.0;
    texCoord.y = (float(((data[3] >> 16) & uint(255)) + (data[3] & uint(1)))) / 16.0;
    #ifndef OPT_SORTTRANSPARENT
    transparency = (data[3] >> 8) & uint(3);
    #endif
    fragPos += vec3(ccoord.x, 0.0, ccoord.y) * 16.0;
    texOffset = float(((data[2] >> 16) & uint(65535)) + ((aniMult / (data[2] & uint(255))) % ((data[2] >> 8) & uint(255))));
    vec3 nat = pow((natLight * 0.8 + skycolor * 0.2) * ((float(data[1] & uint(31)) + 14.0) / 45.0), vec3(2.0));
    light.r = float((data[1] >> 24) & uint(31)) / 31.0;
    light.g = float((data[1] >> 16) & uint(31)) / 31.0;
    light.b = float((data[1] >> 8) & uint(31)) / 31.0;
    light.rgb += nat;
    gl_Position = projection * view * vec4(fragPos, 1.0);
}
