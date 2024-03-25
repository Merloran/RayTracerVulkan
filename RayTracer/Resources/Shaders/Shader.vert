#version 460
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;


layout(binding = 0) uniform UniformBufferObject 
{
	mat4 model;
	mat4 viewProjection;
} ubo;

layout (location = 0) out vec3 worldPosition;
layout (location = 1) out vec3 worldNormal;
layout (location = 2) out vec2 uvFragment;

void main()
{
    worldPosition = vec3(ubo.model * vec4(position, 1.0f));
    worldNormal = mat3(transpose(inverse(ubo.model))) * normal;
	uvFragment = uv;
	
	gl_Position = ubo.viewProjection * vec4(worldPosition, 1.0f);
}