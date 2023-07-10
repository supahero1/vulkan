#version 450

layout(binding = 0) uniform sampler2DArray inTex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) flat in uint inTexIdx;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(inTex, vec3(inTexCoord, inTexIdx));
}
