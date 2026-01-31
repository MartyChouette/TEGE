#version 450

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(skybox, fragTexCoord);
}
