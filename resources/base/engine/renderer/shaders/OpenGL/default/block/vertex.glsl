#version 330
#pragma optimize(on)

layout (location = 0) in uint data1;
// [8 bits: x ([0...255]/16)][8 bits: y ([0...255]/16)][8 bits: z ([0...255]/16)][1 bit: x + 1/16][1 bit: y + 1/16][1 bit: z + 1/16][3 bits: tex map][2 bits: tex coords]
layout (location = 1) in uint data2;
// [8 bits: block id][16 bits: R5G6B5 lighting][8 bits: reserved]

out vec2 TexCoord;
out vec3 FragPos;
out vec4 FragPos2;
out float TexOff;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 ccoord;
uniform uint TexAni;
uniform int isAni;

void main() {
    TexCoord.x = float((data1 >> 1) & uint(1));
    TexCoord.y = float((data1) & uint(1));
    uint TexOff2;
    uint TexID = (data2 >> 24) & uint(255);
    if (isAni != 0) {
        TexOff2 = TexAni;
    } else {
        TexOff2 = (data1 >> 2) & uint(7);
    }
    TexOff = (float(TexID) + float(TexOff2) * 256.0) / 1535.0;
    FragPos.x = (float(((data1 >> 24) & uint(255)) + ((data1 >> 7) & uint(1)))) / 16.0 - 8;
    FragPos.y = (float(((data1 >> 16) & uint(255)) + ((data1 >> 6) & uint(1)))) / 16.0;
    FragPos.z = ((float(((data1 >> 8) & uint(255)) + ((data1 >> 5) & uint(1)))) / 16.0 - 8) * -1;
    FragPos += ccoord * 16;
    gl_Position = projection * view * vec4(FragPos, 1);
    FragPos2 = gl_Position;
}
