#version 130

in vec3		position;
in	vec2	texCoordIn;	// incoming texcoord from the texcoord array
in  vec3	normalIn;
out vec3	viewSpacePosition; 
out vec3	viewSpaceNormal; 
out	vec2	texCoord;	// outgoing interpolated texcoord to fragshader
out vec4	shadowTexCoord;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 lightMatrix;

void main() 
{
	mat4 modelViewMatrix = viewMatrix * modelMatrix; 
	mat4 modelViewProjectionMatrix = projectionMatrix * modelViewMatrix; 
	///////////////////////////////////////////////////////////////////////////
	// The normal matrix should really be the inverse transpose of the 
	// modelViewMatrix, but that doesn't compile on current drivers.
	// Just using the modelView matrix works fine, as long as it does not
	// contain any nonuniform scaling. 
	mat4 normalMatrix = modelViewMatrix; //inverse(transpose(modelViewMatrix));
	///////////////////////////////////////////////////////////////////////////
	gl_Position = modelViewProjectionMatrix * vec4(position,1);
	texCoord = texCoordIn; 
	viewSpacePosition = vec3(modelViewMatrix * vec4(position, 1)); 
	viewSpaceNormal = vec3(normalize((normalMatrix * vec4(normalIn,0.0)).xyz));
	shadowTexCoord = lightMatrix * vec4(viewSpacePosition, 1.0);	
}
