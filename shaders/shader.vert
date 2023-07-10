#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 transform;
} ubo;

layout(location = 0) in vec2 inVertexPosition;
layout(location = 1) in vec2 inTexCoords;

layout(location = 2) in vec3 inPosition;
layout(location = 3) in vec2 inDimensions;
layout(location = 4) in float inRotation;
layout(location = 5) in uint inTexIndex;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) flat out uint outTexIdx;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model *
		vec4(
			vec2(
				inVertexPosition.x * inDimensions.x + inPosition.x,
				inVertexPosition.y * inDimensions.y + inPosition.y
			),
			inPosition.z,
			1.0
		);

    outTexCoord = inTexCoords;
	outTexIdx = inTexIndex;
}
