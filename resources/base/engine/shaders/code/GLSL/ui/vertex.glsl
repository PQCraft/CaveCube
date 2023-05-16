layout (location = 0) in uint data1;
// ui elem: [16 bits: x][16 bits: y]
layout (location = 1) in float data2;
// ui elem: [f32: z]
layout (location = 2) in uint data3;
// ui elem: [16 bits: texture x][16 bits: texture y]
layout (location = 3) in uint data4;
// ui elem: [8 bits: r][8 bits: g][8 bits: b][8 bits: a]
uniform vec2 fbsize;
uniform vec2 texsize;

out vec2 texCoord;
out vec4 color;

void main() {
    gl_Position.x = float((int(data1 >> 16) & int(32767)) + (int(data1 >> 16) & int(32768)) * int(-1));
    gl_Position.x = (gl_Position.x / fbsize.x) * 2.0 - 1.0;
    gl_Position.y = float((int(data1) & int(32767)) + (int(data1) & int(32768)) * int(-1));
    gl_Position.y = (1.0 - (gl_Position.y / fbsize.y)) * 2.0 - 1.0;
    gl_Position.z = data2;
    gl_Position.w = 1.0;
    texCoord.x = float((data3 >> 16) & uint(65535));
    texCoord.y = 1.0 - float(data3 & uint(65535));
    texCoord /= texsize;
    color.r = float((data4 >> 24) & uint(255)) / 255.0;
    color.g = float((data4 >> 16) & uint(255)) / 255.0;
    color.b = float((data4 >> 8) & uint(255)) / 255.0;
    color.a = float(data4 & uint(255)) / 255.0;
}
