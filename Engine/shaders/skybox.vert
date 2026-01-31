#version 450

layout(binding = 0) uniform SkyboxUBO {
    mat4 viewProj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragTexCoord;

void main() {
    fragTexCoord = inPosition;
    vec4 pos = ubo.viewProj * vec4(inPosition * 1000.0, 1.0);
    gl_Position = pos.xyww; // depth = 1.0 (farthest)
}
