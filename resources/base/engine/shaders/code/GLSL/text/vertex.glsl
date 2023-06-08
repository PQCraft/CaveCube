layout (location = 0) in uint data1;
// [16 bits: x][16 bits: y]
layout (location = 1) in float data2;
// [f32: z]
layout (location = 2) in uint data3;
// [8 bits: texture x][8 bits: texture y][8 bits: reserved][8 bits: char]
layout (location = 3) in uint data4;
// [3 bits: 0][1 bit: world colors][1 bit: s][1 bit: u][1 bit: i][1 bit: b][4 bits: fgc][4 bits: bgc][8 bits: fga][8 bits: bga]
uniform vec2 fbsize;
uniform vec2 texsize;
uniform vec3 textColors[16];
uniform vec3 worldTextColors[16];

out vec3 texCoord;
out vec4 fgc;
out vec4 bgc;
flat out uint b;
flat out uint u;
flat out uint s;

void main() {
    gl_Position.x = float((int(data1 >> 16) & int(32767)) + (int(data1 >> 16) & int(32768)) * int(-1));
    gl_Position.x = (gl_Position.x / fbsize.x) * 2.0 - 1.0;
    gl_Position.y = float((int(data1) & int(32767)) + (int(data1) & int(32768)) * int(-1));
    gl_Position.y = (1.0 - (gl_Position.y / fbsize.y)) * 2.0 - 1.0;
    gl_Position.z = data2;
    gl_Position.w = 1.0;
    texCoord.x = float((data3 >> 24) & uint(255));
    texCoord.y = 1.0 - float((data3 >> 16) & uint(255));
    texCoord.xy /= texsize;
    b = (data4 >> 24) & uint(1);
    if (bool((data4 >> 25) & uint(1))) {
        texCoord.x -= 2.0 / texsize.x;
        texCoord.x += (4.0 / texsize.x) * texCoord.y;
    }
    u = (data4 >> 26) & uint(1);
    s = (data4 >> 27) & uint(1);
    texCoord.z = float((data3 >> 8) & uint(255));
    fgc.a = float((data4 >> 8) & uint(255)) / 255.0;
    bgc.a = float(data4 & uint(255)) / 255.0;
    if (bool((data4 >> 28) & uint(1))) {
        fgc.rgb = worldTextColors[(data3 >> 4) & uint(15)];
        bgc.rgb = worldTextColors[data3 & uint(15)];
    } else {
        fgc.rgb = textColors[(data3 >> 4) & uint(15)];
        bgc.rgb = textColors[data3 & uint(15)];
    }
}
