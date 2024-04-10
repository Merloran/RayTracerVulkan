#version 460
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;


layout(binding = 0) uniform UniformBufferObject 
{
	mat4 viewProjection;
	float time;
} ubo;


layout( push_constant ) uniform PushConstants
{
	mat4 model;
} constants;

layout (location = 0) out vec3 worldPosition;
layout (location = 1) out vec3 worldNormal;
layout (location = 2) out vec2 uvFragment;
layout (location = 3) out flat float time;

void main()
{
    worldPosition = vec3(constants.model * vec4(position, 1.0f));
    worldNormal = mat3(transpose(inverse(constants.model))) * normal;
	uvFragment = uv;
	time = ubo.time;
	
	gl_Position = ubo.viewProjection * vec4(worldPosition, 1.0f);
}