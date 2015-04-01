#version 130

in vec3		position;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main() 
{
	mat4 modelViewMatrix = viewMatrix * modelMatrix; 
	mat4 modelViewProjectionMatrix = projectionMatrix * modelViewMatrix; 

	gl_Position = modelViewProjectionMatrix * vec4(position,1);
}