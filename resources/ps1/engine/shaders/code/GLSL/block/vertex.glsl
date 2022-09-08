layout (location = 0) in uint data1;
// [8 bits: X][12 bits: Y][8 bits: Z][1 bit: reserved][1 bit: X + 1][1 bit: Y + 1][1 bit: Z + 1]
layout (location = 1) in uint data2;
// [4 bits: R][4 bits: G][4 bits: B][4 bits: texture X][4 bits: texture Y][2 bits: reserved][1 bit: texture X + 1][1 bit: texture Y + 1][8 bits: reserved]
layout (location = 2) in uint data3;
// [16 bits: texture offset][16 bits: animation offset]
uniform mat4 view;
uniform mat4 projection;
uniform vec2 ccoord;
uniform uint aniMult;
uniform float texmapdiv;

noperspective out vec2 texCoord;
out vec3 fragPos;
out float texOffset;
out vec3 light;

void main() {
    fragPos.x = (float(((data1 >> 24) & uint(255)) + ((data1 >> 2) & uint(1)))) / 16.0 - 8.0;
    fragPos.y = (float(((data1 >> 12) & uint(4095)) + ((data1 >> 1) & uint(1)))) / 16.0;
    fragPos.z = ((float(((data1 >> 4) & uint(255)) + ((data1 >> 0) & uint(1)))) / 16.0 - 8.0) * -1.0;
    texCoord.x = (float(((data2 >> 16) & uint(15)) + ((data2 >> 9) & uint(1)))) / 16.0;
    texCoord.y = (float(((data2 >> 12) & uint(15)) + ((data2 >> 8) & uint(1)))) / 16.0;
    fragPos += vec3(ccoord.x, 0.0, ccoord.y) * 16.0;
    uint texOff2 = (data3 & uint(65535)) * aniMult / uint(65536);
    uint texOff = ((data3 >> 16) & uint(65535)) + uint(mod(float(texOff2), float(data3 & uint(65535))));
    texOffset = float(texOff);
    light.r = float((data2 >> 28) & uint(15)) / 15.0;
    light.g = float((data2 >> 24) & uint(15)) / 15.0;
    light.b = float((data2 >> 20) & uint(15)) / 15.0;
    vec4 newFragPos = projection * view * vec4(fragPos, 1.0);
    newFragPos.xyz /= newFragPos.w * 3;
    float mult = 256;
    newFragPos.xy = floor(mult * newFragPos.xy) / mult;
    newFragPos.xyz *= newFragPos.w * 3;
    gl_Position = newFragPos;
}
