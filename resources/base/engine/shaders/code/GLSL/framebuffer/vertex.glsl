layout (location = 0) in vec4 data;

out vec2 texCoord;

void main() {
    texCoord.x = data[2];
    texCoord.y = data[3];
    gl_Position = vec4(data[0], -data[1], 0.0, 1.0);
}
