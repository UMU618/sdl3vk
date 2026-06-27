#version 450

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 color;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
};

layout(location = 0) out vec3 fragColor;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    gl_Position = mvp * position;
    fragColor = color;
}
