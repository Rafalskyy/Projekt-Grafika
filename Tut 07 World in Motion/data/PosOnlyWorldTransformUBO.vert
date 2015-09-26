#version 330

layout(location = 0) in vec4 position;
layout(location = 5) in vec2 texCoord;

layout(std140) uniform GlobalMatrices
{
	mat4 cameraToClipMatrix;
	mat4 worldToCameraMatrix;
};

uniform mat4 modelToWorldMatrix;

out vec2 colorCoord;

void main()
{
	vec4 temp = modelToWorldMatrix * position;
	temp = worldToCameraMatrix * temp;
	gl_Position = cameraToClipMatrix * temp;
	colorCoord = texCoord;
}
