#version 130

// Note: this is core in OpenGL 3.1 (glsl 1.40) and later, we use OpenGL 3.0 for the tutorials
#extension GL_ARB_texture_rectangle : enable

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform sampler2DRect frameBufferTexture;
uniform sampler2DRect blurredFrameBufferTexture;
uniform float time;
out vec4 fragmentColor;

/**
 * Perturps the sampling coordinates of the pixel and returns the new coordinates
 * these can then be used to sample the frame buffer. The effect uses a sine wave to make us
 * feel woozy.
 */ 
vec2 mushrooms(vec2 inCoord);

/**
 * Samples a region of the frame buffer using gaussian filter weights to blur the image
 * as the kernel width is not that large, it doesnt produce a very large effect. Making it larger
 * is both tedious and expensive, for real time purposes a separable blur is preferable, but this 
 * requires several passes. 
 * takes as input the centre coordinate to sample around.
 */
vec3 blur(vec2 coord);

/**
 * Simply returns the luminance of the input sample color.
 */
vec3 grayscale(vec3 sample);

/**
 * Converts the color sample to sepia tone (by transformation to the yiq color space).
 */
vec3 toSepiaTone(vec3 rgbSample);

/**
 * Changes the sampling coordinates of the pixel so that all pixels within a certain area
 * will have the same coordinate. This creates a mosaic tile effect.
 */
vec2 mosaic(vec2 inCoord);


void main() 
{
	//fragmentColor = vec4(toSepiaTone(texture(frameBufferTexture, gl_FragCoord.xy).xyz), 1.0);

	//fragmentColor = texture(frameBufferTexture, mushrooms(gl_FragCoord.xy));
	
	//fragmentColor = vec4(blur(gl_FragCoord.xy), 1.0);
	//fragmentColor = vec4(grayscale(texture(frameBufferTexture, gl_FragCoord.xy).xyz), 1.0);

	//fragmentColor = texture(frameBufferTexture, mosaic(gl_FragCoord.xy));

	// All at once
	//fragmentColor = vec4(toSepiaTone(blur(mushrooms(mosaic(gl_FragCoord.xy)))), 1.0);

	// Standard
	fragmentColor = texture(frameBufferTexture, gl_FragCoord.xy);

	// Add the bloom effect	
	fragmentColor += texture(blurredFrameBufferTexture, gl_FragCoord.xy) * 2;
}

vec2 mosaic(vec2 inCoord)
{
	vec2 texSize = textureSize(frameBufferTexture);
	float width = texSize.x;
	float height = texSize.y;

	// The number of rows and columns of mosaic rectangles
	int rows = 20;
	int columns = 20;
	
	// Converts this coordinate to the top-right coordinate of the rectangle it belongs to
	inCoord.x = ceil(inCoord.x / (width / columns)) * (width / columns);
	inCoord.y = ceil(inCoord.y / (height / rows)) * (height / rows);

	return inCoord;
}

vec3 toSepiaTone(vec3 rgbSample)
{
	//-----------------------------------------------------------------
	// Variables used for YIQ/RGB color space conversion.
	//-----------------------------------------------------------------
	vec3 yiqTransform0 = vec3(0.299, 0.587, 0.144);
	vec3 yiqTransform1 = vec3(0.596,-0.275,-0.321);
	vec3 yiqTransform2 = vec3(0.212,-0.523, 0.311);

	vec3 yiqInverseTransform0 = vec3(1, 0.956, 0.621);
	vec3 yiqInverseTransform1 = vec3(1,-0.272,-0.647);
	vec3 yiqInverseTransform2 = vec3(1,-1.105, 1.702);
	
	// transform to YIQ color space and set color information to sepia tone
	vec3 yiq = vec3(dot(yiqTransform0, rgbSample), 0.2, 0.0);
	
	// inverse transform to RGB color space
	vec3 result = vec3(dot(yiqInverseTransform0, yiq), dot(yiqInverseTransform1, yiq), dot(yiqInverseTransform2, yiq));
	return result;
}

vec2 mushrooms(vec2 inCoord)
{
	return inCoord + vec2(sin(time * 4.3127 + inCoord.y / 9.0) * 15.0, 0.0);
}

vec3 blur(vec2 coord)
{
	// sample framebuffer texture at current texel
	vec3 result = texture(frameBufferTexture, coord).xyz * 0.5;

	// sample framebuffer texture at neighbouring texels
	result += 0.125 * texture(frameBufferTexture, coord + vec2(1, 0)).xyz;
	result += 0.125 * texture(frameBufferTexture, coord + vec2(0, 1)).xyz;
	result += 0.125 * texture(frameBufferTexture, coord + vec2(-1, 0)).xyz;
	result += 0.125 * texture(frameBufferTexture, coord + vec2(0, -1)).xyz;

	return result;
}

vec3 grayscale(vec3 sample)
{
	return vec3(sample.r * 0.2126 + sample.g * 0.7152 + sample.b * 0.0722);
}
