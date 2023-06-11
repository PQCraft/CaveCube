layout (location = 0) in uvec4 data1;
// 4x [8 bits: X][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1][13 bits: Y][8 bits: Z]
layout (location = 1) in uvec4 data2;
// [16 bits: texture offset][8 bits: animation mod][8 bits: animation divisor]
// [16 bits: (0, 0) texture X, Y][16 bits: (1, 0) texture X, Y]
// [16 bits: (0, 1) texture X, Y][16 bits: (1, 1) texture X, Y]
// [14 bits: 0][2 bits: transparency][4 bits: 0][4 bits: AO texture][8 bits: 4x texture X, Y + 1]
layout (location = 2) in uvec3 data3;
// [3 bits: 0][5 bits: (0, 0) N][3 bits: 0][5 bits: (1, 0) N][3 bits: 0][5 bits: (0, 1) N][3 bits: 0][5 bits: (1, 1) N]
// [1 bit: 0][15 bits: (0, 0) R, G, B][1 bit: 0][15 bits: (1, 0) R, G, B]
// [1 bit: 0][15 bits: (0, 1) R, G, B][1 bit: 0][15 bits: (1, 1) R, G, B]
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
out vec3 light[4];
out vec2 corner;

#define getNLight(s) (pow((natLight * 0.8 + skycolor * 0.2) * ((float((data3[0] >> s) & uint(31)) + 14.0) / 45.0), vec3(2.0)))
#define gsl(d, s) (float((data3[d] >> s) & uint(31)) / 31.0)
#define getLight(d, s1, s2, s3) (vec3(gsl(d, s1), gsl(d, s2), gsl(d, s3)))
#define getTexCoord(d, s1, s2) ((float(((data2[d] >> s1) & uint(255)) + ((data2[3] >> s2) & uint(1)))) * 0.0625)
void main() {
    corner.x = float(uint(gl_VertexID) & uint(1));
    corner.y = float((uint(gl_VertexID) >> 1) & uint(1));
    uint tmpdata;
    tmpdata = data1[uint(gl_VertexID)];
    fragPos.x = (float(((tmpdata >> 24) & uint(255)) + ((tmpdata >> 23) & uint(1)))) * 0.0625 - 8.0;
    fragPos.y = (float(((tmpdata >> 8) & uint(8191)) + ((tmpdata >> 22) & uint(1)))) * 0.0625;
    fragPos.z = ((float((tmpdata & uint(255)) + ((tmpdata >> 21) & uint(1)))) * 0.0625 - 8.0) * -1.0;
    switch (uint(gl_VertexID)) {
        case uint(0):;
            texCoord.x = getTexCoord(1, 24, 7);
            texCoord.y = getTexCoord(1, 16, 6);
            break;
        case uint(1):;
            texCoord.x = getTexCoord(1, 8, 5);
            texCoord.y = getTexCoord(1, 0, 4);
            break;
        case uint(2):;
            texCoord.x = getTexCoord(2, 24, 3);
            texCoord.y = getTexCoord(2, 16, 2);
            break;
        case uint(3):;
            texCoord.x = getTexCoord(2, 8, 1);
            texCoord.y = getTexCoord(2, 0, 0);
            break;
    }
    #ifndef OPT_SORTTRANSPARENT
    transparency = (data2[3] >> 24) & uint(3);
    #endif
    texOffset = float(((data2[0] >> 16) & uint(65535)) + ((aniMult / (data2[0] & uint(255))) % ((data2[0] >> 8) & uint(255))));
    light[0] = vec3(getNLight(24));
    light[1] = vec3(getNLight(16));
    light[2] = vec3(getNLight(8));
    light[3] = vec3(getNLight(0));
    light[0] += getLight(1, 26, 21, 16);
    light[1] += getLight(1, 10, 5, 0);
    light[2] += getLight(2, 26, 21, 16);
    light[3] += getLight(2, 10, 5, 0);
    fragPos += vec3(ccoord.x, 0.0, ccoord.y) * 16.0;
    /*
    if (fragPos.x == 0.0 || fragPos.z == 0.0) {
        light[0] = vec3(0.0);
        light[1] = vec3(0.0);
        light[2] = vec3(0.0);
        light[3] = vec3(0.0);
    }
    */
    gl_Position = projection * view * vec4(fragPos, 1.0);
}
