#version 130
// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// inputs from vertex shader.
in vec2 texCoord;
in vec3 viewSpacePosition; 
in vec3 viewSpaceNormal; 
in vec4 shadowTexCoord;

// output to frame buffer.
out vec4 fragmentColor;

// global uniforms, that are the same for the whole scene
uniform sampler2DShadow shadowMap;
uniform samplerCube cubeMap; 
uniform vec3 scene_ambient_light = vec3(0.05, 0.05, 0.05);
uniform vec3 scene_light = vec3(0.6, 0.6, 0.6);

// object specific uniforms, change once per object but are the same for all materials in object.
uniform float object_alpha; 
uniform float object_reflectiveness = 0.0;

// matrial properties, changed when material changes.
uniform float material_shininess;
uniform vec3 material_diffuse_color; 
uniform vec3 material_specular_color; 
//uniform vec3 material_ambient_color;
uniform vec3 material_emissive_color; 
uniform int has_diffuse_texture; 
uniform sampler2D diffuse_texture;

uniform vec3 viewSpaceLightPosition;
uniform mat4 inverseViewNormalMatrix;

vec3 calculateAmbient(vec3 ambientLight, vec3 materialAmbient)
{
	return ambientLight * materialAmbient;
}

vec3 calculateDiffuse(vec3 diffuseLight, vec3 materialDiffuse, vec3 normal, vec3 directionToLight)
{
	float intensity = max(0, dot(normal, directionToLight));
	return diffuseLight * materialDiffuse * intensity;
}

vec3 calculateSpecular(vec3 specularLight, vec3 materialSpecular, float materialShininess,
						vec3 normal, vec3 directionToLight, vec3 directionFromEye)
{
	vec3 h = normalize(directionToLight - directionFromEye);	
	float normalizationFactor = ((materialShininess + 2.0) / 8.0);
	return specularLight * materialSpecular
		* pow(max(0, dot(h, normal)), materialShininess)
		* normalizationFactor;
}

vec3 calculateFresnel(vec3 materialSpecular, vec3 normal, vec3 directionFromEye)
{
	return materialSpecular + (vec3(1.0) - materialSpecular)
			* pow(clamp(1.0 + dot(directionFromEye, normal), 0.0, 1.0), 5.0);
}


void main() 
{
	vec3 diffuse = material_diffuse_color;
	vec3 specular = material_specular_color;
	// The emissive term allows objects to glow irrespective of illumination, this is just added
	// to the shading, most materials have an emissive color of 0, in the scene the sky uses an emissive of 1
	// which allows it a constant and uniform look.
	vec3 emissive = material_emissive_color;
	// Note: we do not use the per-material ambient. This is because it is more
	// practical to control on a scene basis, and is usually the same as diffuse.
	// Feel free to enable it, but then it must be correctly set for _all_ materials (in the .mtl files)!
	vec3 ambient = material_diffuse_color;//material_ambient_color;
	
	// if we have a texture we modulate all of the color properties
	if (has_diffuse_texture == 1)
	{
		diffuse *= texture(diffuse_texture, texCoord.xy).xyz; 
		ambient *= texture(diffuse_texture, texCoord.xy).xyz; 
		emissive *= texture(diffuse_texture, texCoord.xy).xyz; 
	}

	vec3 normal = normalize(viewSpaceNormal);
	vec3 directionToLight = 
			normalize(viewSpaceLightPosition - viewSpacePosition);
	vec3 directionFromEye = normalize(viewSpacePosition);
	
	vec3 reflectionVector = (inverseViewNormalMatrix *
							vec4(reflect(directionFromEye, normal), 0.0)).xyz;
	vec3 envMapSample = texture(cubeMap, reflectionVector).rgb;

	vec3 fresnelSpecular = calculateFresnel(specular, normal,
											directionFromEye);

	float visibility = textureProj(shadowMap, shadowTexCoord);
	vec3 shading = calculateAmbient(scene_ambient_light, ambient)
		+ calculateDiffuse(scene_light, diffuse, normal, directionToLight) * visibility
		+ calculateSpecular(scene_light, specular, material_shininess,
							normal, directionToLight, directionFromEye) * visibility
		+ emissive
		+ envMapSample * fresnelSpecular * object_reflectiveness * visibility;
	fragmentColor = vec4(shading, object_alpha);
}
