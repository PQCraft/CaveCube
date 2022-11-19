layout (location = 0) in uint data1;
// ui elem: [16 bits: x][16 bits: y]
// text:    [16 bits: x][16 bits: y]
layout (location = 1) in uint data2;
// ui elem: [1 bit: 0][23 bits: reserved][8 bits: z]
// text:    [1 bit: 1][4 bits: reserved][1 bit: strikethrough][1 bit: underline][1 bit: italic][1 bit: bold][8 bits: reserved (16-bit charset maybe)][8 bits: text char][8 bits: z]
layout (location = 2) in uint data3;
// ui elem: [8 bits: r][8 bits: g][8 bits: b][8 bits: a]
// text:    [8 bits: fgc alpha][8 bits: bgc alpha][4 bits: text fgc][4 bits: text bgc][8 bits: reserved]
layout (location = 3) in uint data4;
// ui elem: [32 bits: 0]
// text:    [8 bits: texture x][8 bits: texture y][8 bits: max texture x][8 bits: max texture y]
uniform float xsize;
uniform float ysize;
uniform vec3 textColor[16];
uniform vec2 uitexdiv;
uniform float scale;

out float texNum;
out vec4 mcolor;
out vec4 bmcolor;
out float chartw;
out float charth;
out float chartx;
out float charty;
flat out uint elemType;
flat out uint b;
flat out uint u;
flat out uint s;

void main() {
    float x = float(int(-32768) * int((data1 >> 31) & uint(1)) + int((data1 >> 16) & uint(32767)));
    float y = float(int(-32768) * int((data1 >> 15) & uint(1)) + int(data1 & uint(32767)));
    float z = 1.0 - (float(int(-128) * int((data2 >> 7) & uint(1)) + int(data2 & uint(127))) / 256.0 + 0.5);
    if ((elemType = ((data2 >> 31) & uint(1))) == uint(1)) {
        texNum = float((data2 >> 8) & uint(255));
        chartw = float((data4 >> 8) & uint(255));
        charth = float(data4 & uint(255));
        chartx = float((data4 >> 24) & uint(255));
        charty = float((data4 >> 16) & uint(255));
        b = (data2 >> 24) & uint(1);
        if (bool((data2 >> 25) & uint(1))) {
            x += (1.0 - ((charty / charth) * 2.0)) * 4.0 * scale;
        }
        u = (data2 >> 26) & uint(1);
        s = (data2 >> 27) & uint(1);
        mcolor = vec4(textColor[uint((data3 >> 12) & uint(15))], float((data3 >> 24) & uint(255)) / 255.0);
        bmcolor = vec4(textColor[uint((data3 >> 8) & uint(15))], float((data3 >> 16) & uint(255)) / 255.0);
    } else {
        mcolor.r = float((data3 >> 24) & uint(255)) / 255.0;
        mcolor.g = float((data3 >> 16) & uint(255)) / 255.0;
        mcolor.b = float((data3 >> 8) & uint(255)) / 255.0;
        mcolor.a = float(data3 & uint(255)) / 255.0;
    }
    gl_Position = vec4((x * 2.0) / xsize - 1.0, 1.0 - (y * 2.0) / ysize, z, 1.0);
}
