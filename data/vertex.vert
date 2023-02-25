#version 450
#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 color;

layout(binding = 0) uniform Matrices {
    mat4 projection;
    mat4 view;
    mat4 model;
} matrices;

void main()
{
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(inPosition, 1);
    color = inColor;
}
