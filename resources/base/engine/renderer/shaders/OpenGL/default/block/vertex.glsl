#version 330
#pragma optimize(on)

layout (location = 0) in uint data;
//[1 bit: x + 1][4 bits: x][1 bit: y + 1][4 bits: y][1 bit: z + 1][4 bits: z][3 bits: tex map][2 bits: tex coords][4 bits: lighting][8 bits: block id]

out vec2 TexCoord;
out vec3 FragPos;
out float TexOff;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 ccoord;
uniform uint TexAni;
uniform int isAni;

void main() {
    TexCoord.x = float((data >> 13) & uint(1));
    TexCoord.y = float((data >> 12) & uint(1));
    uint TexOff2;
    uint TexID = data & uint(255);
    if (isAni != 0) {
        TexOff2 = TexAni;
    } else {
        TexOff2 = (data >> 14) & uint(7);
    }
    TexOff = (float(TexID) + float(TexOff2) * 256.0) / 1535.0;
    FragPos.x = float(((data >> 27) & uint(15)) + ((data >> 31) & uint(1))) - 8;
    FragPos.y = float(((data >> 22) & uint(15)) + ((data >> 26) & uint(1)));
    FragPos.z = (float(((data >> 17) & uint(15)) + ((data >> 21) & uint(1))) - 8) * -1;
    FragPos += ccoord * 16;
    gl_Position = projection * view * vec4(FragPos, 1);
}
