layout (location = 0) in uint data1;
// [16 bits: x][16 bits: y]
layout (location = 1) in uint data2;
// [8 bits: char][8 bits: reserved][1 bit: texture x][1 bit: texture y][14: reserved]
uniform float xsize;
uniform float ysize;

out vec2 texCoord;
out float chr;

void main() {
    chr = float((data2 >> 24) & uint(255));
    texCoord.x = float((data2 >> 15) & uint(1));
    texCoord.y = float((data2 >> 14) & uint(1));
    float x = float(int((data1 >> 16) & uint(65535))) / xsize * 2.0 - 1.0;
    float y = 1.0 - float(int(data1 & uint(65535))) / ysize * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
