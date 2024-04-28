#version 460
layout (location = 0) in vec2 uvFragment;

layout (set = 4, binding = 0) uniform sampler2D screenImage;

layout (location = 0) out vec4 color;

void main()
{
	color = texture(screenImage, uvFragment);
}
