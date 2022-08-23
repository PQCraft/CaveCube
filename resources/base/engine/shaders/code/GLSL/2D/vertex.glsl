layout (location = 0) in vec4 data;
uniform float xratio;
uniform float yratio;

out vec2 texCoord;

void main() {
    texCoord.x = data[2];
    texCoord.y = data[3];
    gl_Position = vec4(data[0] * xratio, data[1] * yratio, 0.0, 1.0);
}
