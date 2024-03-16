#version 460
layout(location = 0) in vec2 pixelUV;

layout(location = 0) out vec4 colorOutput;

void main() 
{
	colorOutput = vec4(pixelUV, 0.0f, 1.0f);
}