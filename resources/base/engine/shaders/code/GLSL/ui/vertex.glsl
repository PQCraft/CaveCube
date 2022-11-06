layout (location = 0) in uint data1;
// ui elem: [16 bits: x][16 bits: y]
// text:    [16 bits: x][16 bits: y]
layout (location = 1) in uint data2;
// ui elem: [1 bit: 0][7 bits: reserved][8 bits: elem id][8 bits: alpha][8 bits: z]
// text:    [1 bit: 1][5 bits: reserved][1 bit: texture x][1 bit: texture y][8 bits: text char][8 bits: reserved][8 bits: z]
layout (location = 2) in uint data3;
// ui elem: [16 bits: texture x][16 bits: texture y]
// text:    [8 bits: fgc alpha][8 bits: bgc alpha][4 bits: text fgc][4 bits: text bgc][8 bits: reserved]
uniform float xsize;
uniform float ysize;
uniform vec3 textColor[16];
uniform vec2 uitexdiv;

out vec2 texCoord;
out float texNum;
out vec4 mcolor;
out vec4 bmcolor;
flat out uint texIndex;

void main() {
    if ((texIndex = ((data2 >> 31) & uint(1))) == uint(1)) {
        float x = float(int((data1 >> 16) & uint(65535))) / xsize * 2.0 - 1.0;
        float y = 1.0 - float(int(data1 & uint(65535))) / ysize * 2.0;
        float z = float(int(-128) * int((data2 >> 7) & uint(127)) + int(data2 & uint(127)));
        gl_Position = vec4(x, y, z, 1.0);
        texNum = float((data2 >> 16) & uint(255));
        texCoord.x = float((data2 >> 25) & uint(1));
        texCoord.y = float((data2 >> 24) & uint(1));
        mcolor = vec4(textColor[uint((data3 >> 12) & uint(15))], float((data3 >> 24) & uint(8)) / 255.0);
        bmcolor = vec4(textColor[uint((data3 >> 8) & uint(15))], float((data3 >> 16) & uint(8)) / 255.0);
    } else {
        float x = float(int((data1 >> 16) & uint(65535))) / xsize * 2.0 - 1.0;
        float y = 1.0 - float(int(data1 & uint(65535))) / ysize * 2.0;
        float z = float(int(-128) * int((data2 >> 7) & uint(127)) + int(data2 & uint(127)));
        gl_Position = vec4(x, y, z, 1.0);
        texNum = float((data2 >> 16) & uint(255));
        texCoord.x = float((data3 >> 16) & uint(65535)) / uitexdiv.x;
        texCoord.y = float((data3) & uint(65535)) / uitexdiv.y;
        mcolor = vec4(1.0, 1.0, 1.0, float((data2 >> 8) & uint(8)) / 255.0);
        bmcolor = vec4(1.0, 1.0, 1.0, 0.0);
    }
}
