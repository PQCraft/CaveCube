layout (location = 0) in vec4 data;

out vec2 texCoord;

uniform vec2 mpos;
uniform float xratio;
uniform float yratio;
uniform float scale;

void main() {
    texCoord.x = data[2];
    texCoord.y = data[3];
    gl_Position = vec4((data[0] * xratio * scale) + mpos.x * 2.0 + xratio * scale - 1.0, 1.0 - (((1.0 - data[1]) * yratio * scale) + mpos.y * 2.0), 0.0, 1.0);
}
