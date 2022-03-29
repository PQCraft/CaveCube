#version 330
#pragma optimize(on)

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragPos;

uniform mat4 model;
uniform vec3 mPos;
uniform mat4 view;
uniform mat4 projection;

uniform int is2D = 0;

void main() {
    TexCoord = aTexCoord;
    //mat4 model2 = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, -1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
    FragPos = aPos + mPos;
    if (is2D == 0) {
        FragPos.z *= -1.0;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    } else {
        gl_Position = vec4(FragPos, 1.0);
    }
}
