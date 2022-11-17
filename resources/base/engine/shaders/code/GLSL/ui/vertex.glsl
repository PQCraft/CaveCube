layout (location = 0) in uint data1;
// ui elem: [16 bits: x][16 bits: y]
// text:    [16 bits: x][16 bits: y]
layout (location = 1) in uint data2;
// ui elem: [1 bit: 0][23 bits: reserved][8 bits: z]
// text:    [1 bit: 1][5 bits: reserved][1 bit: texture x][1 bit: texture y][8 bits: reserved (16-bit charset maybe)][8 bits: text char][8 bits: z]
layout (location = 2) in uint data3;
// ui elem: [8 bits: r][8 bits: g][8 bits: b][8 bits: a]
// text:    [8 bits: fgc alpha][8 bits: bgc alpha][4 bits: text fgc][4 bits: text bgc][8 bits: reserved]
layout (location = 3) in uint data4;
// ui elem: [32 bits: 0]
// text:    [8 bits: left clip][8 bits: right clip][8 bits: top clip][8 bits: bottom clip]
uniform float xsize;
uniform float ysize;
uniform vec3 textColor[16];
uniform vec2 uitexdiv;

out vec2 texCoord;
out float texNum;
out vec4 mcolor;
out vec4 bmcolor;
flat out uint elemType;

void main() {
    float x = float(int(-32768) * int((data1 >> 31) & uint(1)) + int((data1 >> 16) & uint(32767))) / xsize;
    float y = float(int(-32768) * int((data1 >> 15) & uint(1)) + int(data1 & uint(32767))) / ysize;
    float z = 1.0 - (float(int(-128) * int((data2 >> 7) & uint(1)) + int(data2 & uint(127))) / 256.0 + 0.5);
    gl_Position = vec4(x * 2.0 - 1.0, 1.0 - y * 2.0, z, 1.0);
    if ((elemType = ((data2 >> 31) & uint(1))) == uint(1)) {
        texNum = float((data2 >> 8) & uint(255));
        texCoord.x = float((data2 >> 25) & uint(1));
        texCoord.y = float((data2 >> 24) & uint(1));
        mcolor = vec4(textColor[uint((data3 >> 12) & uint(15))], float((data3 >> 24) & uint(255)) / 255.0);
        bmcolor = vec4(textColor[uint((data3 >> 8) & uint(15))], float((data3 >> 16) & uint(255)) / 255.0);
    } else {
        mcolor.r = float((data3 >> 24) & uint(255)) / 255.0;
        mcolor.g = float((data3 >> 16) & uint(255)) / 255.0;
        mcolor.b = float((data3 >> 8) & uint(255)) / 255.0;
        mcolor.a = float(data3 & uint(255)) / 255.0;
    }
}
