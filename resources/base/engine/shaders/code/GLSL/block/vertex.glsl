layout (location = 0) in uint data1;
// [8 bits: X][16 bits: Y][8 bits: Z]
layout (location = 1) in uint data2;
// [3 bits: reserved][5 bits: R][3 bits: reserved][5 bits: G][3 bits: reserved][5 bits: B][3 bits: reserved][5 bits: natural]
layout (location = 2) in uint data3;
// [16 bits: texture offset][8 bits: animation offset][8 bits: animation divisor]
layout (location = 3) in uint data4;
// [8 bits: texture X][8 bits: texture Y][6 bits: reserved][2 bits: transparency][3 bits: reserved][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1][1 bit: texture X + 1][1 bit: texture Y + 1]
uniform mat4 view;
uniform mat4 projection;
uniform vec2 ccoord;
uniform uint aniMult;
uniform vec3 natLight;
uniform vec3 skycolor;

out vec2 texCoord;
flat out uint transparency;
out vec3 fragPos;
out float texOffset;
out vec3 light;

void main() {
    fragPos.x = (float(((data1 >> 24) & uint(255)) + ((data4 >> 4) & uint(1)))) / 16.0 - 8.0;
    fragPos.y = (float(((data1 >> 8) & uint(65535)) + ((data4 >> 3) & uint(1)))) / 16.0;
    fragPos.z = ((float((data1 & uint(255)) + ((data4 >> 2) & uint(1)))) / 16.0 - 8.0) * -1.0;
    texCoord.x = (float(((data4 >> 24) & uint(255)) + ((data4 >> 1) & uint(1)))) / 16.0;
    texCoord.y = (float(((data4 >> 16) & uint(255)) + (data4 & uint(1)))) / 16.0;
    transparency = (data4 >> 8) & uint(3);
    fragPos += vec3(ccoord.x, 0.0, ccoord.y) * 16.0;
    texOffset = float(((data3 >> 16) & uint(65535)) + ((aniMult / (data3 & uint(255))) % ((data3 >> 8) & uint(255))));
    vec3 nat = (natLight * 0.8 + skycolor * 0.2) * (float(data2 & uint(31)) / 31.0);
    light.r = float((data2 >> 24) & uint(31)) / 31.0;
    light.g = float((data2 >> 16) & uint(31)) / 31.0;
    light.b = float((data2 >> 8) & uint(31)) / 31.0;
    light.rgb += nat;
    gl_Position = projection * view * vec4(fragPos, 1.0);
}
