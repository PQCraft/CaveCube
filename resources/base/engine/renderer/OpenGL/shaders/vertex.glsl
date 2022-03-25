#version 330
#pragma optimize(on)

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform int is2D;

void main() {
    TexCoord = aTexCoord;
    FragPos = vec3(model * vec4(aPos, 1.0));
    if (is2D == 0) {
        gl_Position = projection * view * vec4(FragPos, 1.0);
    } else {
        gl_Position = vec4(FragPos, 1.0);
    }
}
