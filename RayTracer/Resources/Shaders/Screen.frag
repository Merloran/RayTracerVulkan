#version 460
layout (location = 0) in vec2 uvFragment;

layout (rgba32f, set = 3, binding = 0) readonly uniform image2D accumulated;

layout( push_constant ) uniform PushConstants
{
	float invFrameCount;
} constants;

layout (location = 0) out vec4 color;

void main()
{ 
	vec3 c = texture(accumulated, uvFragment).rgb * constants.invFrameCount;
	c = sqrt(c);
    color = vec4(c, 1.0f);
}
