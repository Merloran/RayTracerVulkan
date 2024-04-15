#version 460
layout (location = 0) in vec2 uvFragment;

layout (rgba32f, set = 3, binding = 0) uniform image2D accumulated;

layout( push_constant ) uniform PushConstants
{
	ivec2 imageSize;
	float invFrameCount;
} constants;

layout (location = 0) out vec4 color;

void main()
{ 
	ivec2 uv = ivec2(constants.imageSize.xy * uvFragment);
	vec3 c = imageLoad(accumulated, uv).rgb * constants.invFrameCount;
	c = sqrt(c);
    color = vec4(c, 1.0f);
}
