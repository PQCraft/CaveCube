layout (location = 0) in uint data1;
// [8 bits: X][12 bits: Y][8 bits: Z][1 bit: reserved][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1]
layout (location = 1) in uint data2;
// [4 bits: reserved][4 bits: R][4 bits: G][4 bits: B][4 bits: reserved][4 bits: nat R][4 bits: nat G][4 bits: nat B]
layout (location = 2) in uint data3;
// [16 bits: texture offset][8 bits: animation offset][8 bits: animation divisor]
layout (location = 3) in uint data4;
// [8 bits: texture X][8 bits: texture Y][14 bits: reserved][1 bit: texture X + 1][1 bit: texture Y + 1]
uniform mat4 view;
uniform mat4 projection;
uniform vec2 ccoord;
uniform uint aniMult;
uniform vec3 natLight;

out vec2 texCoord;
out vec3 fragPos;
out float texOffset;
out vec3 light;

void main() {
    fragPos.x = (float(((data1 >> 24) & uint(255)) + ((data1 >> 2) & uint(1)))) / 16.0 - 8.0;
    fragPos.y = (float(((data1 >> 12) & uint(4095)) + ((data1 >> 1) & uint(1)))) / 16.0;
    fragPos.z = ((float(((data1 >> 4) & uint(255)) + (data1 & uint(1)))) / 16.0 - 8.0) * -1.0;
    texCoord.x = (float(((data4 >> 24) & uint(255)) + ((data4 >> 1) & uint(1)))) / 16.0;
    texCoord.y = (float(((data4 >> 16) & uint(255)) + (data4 & uint(1)))) / 16.0;
    fragPos += vec3(ccoord.x, 0.0, ccoord.y) * 16.0;
    texOffset = float(((data3 >> 16) & uint(65535)) + ((aniMult / (data3 & uint(255))) % ((data3 >> 8) & uint(255))));
    light.r = (float((data2 >> 24) & uint(15)) / 15.0) + natLight.r * (float((data2 >> 8) & uint(15)) / 15.0);
    light.g = (float((data2 >> 20) & uint(15)) / 15.0) + natLight.g * (float((data2 >> 4) & uint(15)) / 15.0);
    light.b = (float((data2 >> 16) & uint(15)) / 15.0) + natLight.b * (float(data2 & uint(15)) / 15.0);
    //light = clamp(light, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    gl_Position = projection * view * vec4(fragPos, 1.0);
}
