layout (location = 0) in uint data1;
// ui elem: [16 bits: x][16 bits: y]
// text:    [16 bits: x][16 bits: y]
layout (location = 1) in uint data2;
// ui elem: []
// text:    [8 bits: text char][8 bits: reserved][4 bits: text fgc][4 bits: text bgc][8 bits: reserved]
layout (location = 2) in uint data3;
// ui elem: []
// text:    [8 bits: fgc alpha][8 bits: bgc alpha]
// [16 bits: texture x][16 bits: texture y][8 bits: alpha][2 bits: reserved][2 bits: type]
uniform float xsize;
uniform float ysize;

out vec2 texCoord;
out float chr;
out vec4 mcolor;
out vec4 bmcolor;

void main() {
    chr = float((data2 >> 24) & uint(255));
    texCoord.x = float((data2 >> 15) & uint(1));
    texCoord.y = float((data2 >> 14) & uint(1));
    float x = float(int((data1 >> 16) & uint(65535))) / xsize * 2.0 - 1.0;
    float y = 1.0 - float(int(data1 & uint(65535))) / ysize * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
