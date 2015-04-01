#ifdef WIN32
#include <windows.h>
#endif


#include <GL/glew.h>
#include <GL/freeglut.h>

#include <IL/il.h>
#include <IL/ilut.h>

#include <stdlib.h>
#include <algorithm>

#include <OBJModel.h>
#include <glutil.h>
#include <float4x4.h>
#include <float3x3.h>

using namespace std;
using namespace chag;

//*****************************************************************************
//	Global variables
//*****************************************************************************
bool paused = false;				// Tells us wether sun animation is paused
float currentTime = 0.0f;		// Tells us the current time
GLuint shaderProgram, postFxShader, horizontalBlurShader,
		verticalBlurShader, cutoffShader;
const float3 up = {0.0f, 1.0f, 0.0f};
GLuint cubeMapTexture;
GLuint cubeMap2;
//*****************************************************************************
//	OBJ Model declarations
//*****************************************************************************
OBJModel *world; 
OBJModel *water; 
OBJModel *skybox; 
OBJModel *skyboxnight; 
OBJModel *car; 

//*****************************************************************************
//	Camera state variables (updated in motion())
//*****************************************************************************
float camera_theta = M_PI / 6.0f;
float camera_phi = M_PI / 4.0f;
float camera_r = 30.0; 
float camera_target_altitude = 5.2; 

//*****************************************************************************
//	Light state variables (updated in idle())
//*****************************************************************************
float3 lightPosition;

//*****************************************************************************
//	Mouse input state variables
//*****************************************************************************
bool leftDown = false;
bool middleDown = false;
bool rightDown = false;
int prev_x = 0;
int prev_y = 0;

//*****************************************************************************
//	Shadow map
//*****************************************************************************
GLuint shadowShaderProgram;
GLuint shadowMapTexture;
GLuint shadowMapFBO;
const int shadowMapResolution = 1024;

struct FBOInfo
{
	GLuint id;
	GLuint colorTextureTarget;
	GLuint depthBuffer;
	int width;
	int height;
};

FBOInfo postProcessFBO;
FBOInfo horizontalBlurFBO;
FBOInfo verticalBlurFBO;
FBOInfo cutoffFBO;

void createShadowMap(int width, int height);
void drawFullScreenQuad();
FBOInfo createPostProcessFBO(int width, int height);
void renderBlur();

// Helper function to turn spherical coordinates into cartesian (x,y,z)
float3 sphericalToCartesian(float theta, float phi, float r)
{
	return make_vector( r * sinf(theta)*sinf(phi),
					 	r * cosf(phi), 
						r * cosf(theta)*sinf(phi) );
}


void initGL()
{
	// Initialize GLEW, which provides access to OpenGL Extensions
	glewInit();  

	// Print infomation about GL and ensure that we've got GL.30
	startupGLDiagnostics();

	ilInit();					// initialize devIL (developers Image Library)
	ilutRenderer(ILUT_OPENGL);  // initialize devIL

	// Workaround for AMD, which hopefully will not be neccessary in the near future...
	if (!glBindFragDataLocation)
	{
		glBindFragDataLocation = glBindFragDataLocationEXT;
	}

	//*************************************************************************
	//	Load shaders
	//*************************************************************************
	shaderProgram = loadShaderProgram("shaders/shader.vert", "shaders/shader.frag");
	glBindAttribLocation(shaderProgram, 0, "position"); 	
	glBindAttribLocation(shaderProgram, 2, "texCoordIn");
	glBindAttribLocation(shaderProgram, 1, "normalIn");
	glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
	linkShaderProgram(shaderProgram);

	shadowShaderProgram = loadShaderProgram("shaders/shadow.vert", "shaders/shadow.frag");
	glBindAttribLocation(shadowShaderProgram, 0, "position"); 	
	glBindFragDataLocation(shadowShaderProgram, 0, "fragmentColor");
	linkShaderProgram(shadowShaderProgram);

	// load and set up post processing shader
 	postFxShader = loadShaderProgram("shaders/postFx.vert", "shaders/postFx.frag");
	glBindAttribLocation(postFxShader, 0, "position");	
	glBindFragDataLocation(postFxShader, 0, "fragmentColor");
	linkShaderProgram(postFxShader);
	CHECK_GL_ERROR();

	// load and set up horizontal blur shader
 	horizontalBlurShader = loadShaderProgram("shaders/postFx.vert", "shaders/horizontal_blur.frag");
	glBindAttribLocation(horizontalBlurShader, 0, "position");
	glBindFragDataLocation(horizontalBlurShader, 0, "fragmentColor");
	linkShaderProgram(horizontalBlurShader);
	CHECK_GL_ERROR();

	// load and set up vertical blur shader
 	verticalBlurShader = loadShaderProgram("shaders/postFx.vert", "shaders/vertical_blur.frag");	
	glBindAttribLocation(verticalBlurShader, 0, "position");
	glBindFragDataLocation(verticalBlurShader, 0, "fragmentColor");
	linkShaderProgram(verticalBlurShader);
	CHECK_GL_ERROR();

	// load and set up cutoff shader
 	cutoffShader = loadShaderProgram("shaders/postFx.vert", "shaders/cutoff.frag");	
	glBindAttribLocation(cutoffShader, 0, "position");
	glBindFragDataLocation(cutoffShader, 0, "fragmentColor");
	linkShaderProgram(cutoffShader);
	CHECK_GL_ERROR();

	glEnable(GL_DEPTH_TEST);	// enable Z-buffering 
	glEnable(GL_CULL_FACE);		// enable backface culling

	// Create the shadow map
	createShadowMap(shadowMapResolution, shadowMapResolution);
	
	// Create the cube map
	cubeMapTexture = loadCubeMap("cube0.png", "cube1.png",
								"cube2.png", "cube3.png",
								"cube4.png", "cube5.png");

	setUniformSlow(shaderProgram, "cubeMap", 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);

	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);

	// Post processing
	postProcessFBO = createPostProcessFBO(w, h);

	// Cutoff
	cutoffFBO = createPostProcessFBO(w, h);

	// Blur
	horizontalBlurFBO = createPostProcessFBO(w, h);
	verticalBlurFBO = createPostProcessFBO(w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	CHECK_GL_ERROR();

	//*************************************************************************
	// Load the models from disk
	//*************************************************************************
	world = new OBJModel(); 
	world->load("../scenes/island2.obj");
	skybox = new OBJModel();
	skybox->load("../scenes/skybox.obj");
	skyboxnight = new OBJModel();
	skyboxnight->load("../scenes/skyboxnight.obj");
	// Make the textures of the skyboxes use clamp to edge to avoid seams
	for(int i=0; i<6; i++){
		glBindTexture(GL_TEXTURE_2D, skybox->getDiffuseTexture(i)); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, skyboxnight->getDiffuseTexture(i)); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	water = new OBJModel(); 
	water->load("../scenes/water.obj");
	car = new OBJModel(); 
	car->load("../scenes/car.obj");
	CHECK_GL_ERROR();
}

void drawModel(GLuint shaderProgram, OBJModel *model, const float4x4 &modelMatrix)
{
	setUniformSlow(shaderProgram, "modelMatrix", modelMatrix);
	model->render();
}

/**
* In this function, add all scene elements that should cast shadow, that way
* there is only one draw call to each of these, as this function is called twice.
*/
void drawShadowCasters(GLuint shaderProgram, const float4x4 &viewMatrix, const float4x4 &projectionMatrix)
{
	drawModel(shaderProgram, world, make_identity<float4x4>());
	setUniformSlow(shaderProgram, "object_reflectiveness", 0.5f); 
	drawModel(shaderProgram, car, make_translation(make_vector(0.0f, 0.0f, 0.0f))); 
	setUniformSlow(shaderProgram, "object_reflectiveness", 0.0f); 
}

void drawShadowMap(const float4x4 &viewMatrix, const float4x4 &projectionMatrix)
{
	glPolygonOffset(2.5, 10);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glViewport(0, 0, shadowMapResolution, shadowMapResolution);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	// Shader Program
	GLint current_program; 
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
	glUseProgram(shadowShaderProgram);

	setUniformSlow(shadowShaderProgram, "viewMatrix", viewMatrix);
	setUniformSlow(shadowShaderProgram, "projectionMatrix", projectionMatrix);

	drawShadowCasters(shadowShaderProgram, viewMatrix, projectionMatrix);

	glUseProgram(current_program);	

	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	CHECK_GL_ERROR();
}

void drawScene(const float4x4 &lightViewMatrix, const float4x4 &lightProjectionMatrix)
{
	//*************************************************************************
	// Render the scene from the cameras viewpoint
	//*************************************************************************
	glClearColor(0.2,0.2,0.8,1.0);						
	glClearDepth(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);
	glViewport(0, 0, w, h);
	// Use shader and set up uniforms
	glUseProgram(shaderProgram);
	float3 camera_position = sphericalToCartesian(camera_theta, camera_phi, camera_r);
	float3 camera_lookAt = make_vector(0.0f, camera_target_altitude, 0.0f);
	float3 camera_up = make_vector(0.0f, 1.0f, 0.0f);
	float4x4 viewMatrix = lookAt(camera_position, camera_lookAt, camera_up);
	float4x4 projectionMatrix = perspectiveMatrix(45.0f, float(w) / float(h), 0.1f, 1000.0f);
	setUniformSlow(shaderProgram, "viewMatrix", viewMatrix);
	setUniformSlow(shaderProgram, "projectionMatrix", projectionMatrix);
	setUniformSlow(shaderProgram, "inverseViewNormalMatrix", transpose(viewMatrix));

	drawModel(shaderProgram, water, make_translation(make_vector(0.0f, -6.0f, 0.0f)));

	// Shadows
	float3 viewSpaceLightPos = transformPoint(viewMatrix, lightPosition);
	setUniformSlow(shaderProgram, "viewSpaceLightPosition", viewSpaceLightPos);

	float4x4 lightMatrix = make_translation(make_vector(0.5f, 0.5f, 0.5f)) *
		make_scale<float4x4>(0.5f) * lightProjectionMatrix *
		lightViewMatrix	* inverse(viewMatrix);
	setUniformSlow(shaderProgram, "lightMatrix", lightMatrix);

	drawShadowCasters(shaderProgram, viewMatrix, projectionMatrix);

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	drawModel(shaderProgram, skyboxnight, make_identity<float4x4>());
	setUniformSlow(shaderProgram, "object_alpha", max<float>(0.0f, cosf((currentTime / 20.0f) * 2.0f * M_PI))); 
	drawModel(shaderProgram, skybox, make_identity<float4x4>());
	setUniformSlow(shaderProgram, "object_alpha", 1.0f);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE); 

	glUseProgram(0);	
	CHECK_GL_ERROR();
}



void display(void)
{
	int w = glutGet((GLenum)GLUT_WINDOW_WIDTH);
	int h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);

	// Set up view and projection matrices for light
	float4x4 lightViewMatrix = lookAt(lightPosition, make_vector(0.0f, 0.0f, 0.0f), up);
	float4x4 lightProjectionMatrix = perspectiveMatrix(25.0f, 1.0, 5.0f, 500.0f);

	drawShadowMap(lightViewMatrix, lightProjectionMatrix);
	
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO.id);

	drawScene(lightViewMatrix, lightProjectionMatrix);

	// Render with cutoffShader
	glBindFramebuffer(GL_FRAMEBUFFER, cutoffFBO.id);
	glClearColor(0.0,0.0,0.0,1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(cutoffShader);
	setUniformSlow(cutoffShader, "frameBufferTexture", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postProcessFBO.colorTextureTarget);
	drawFullScreenQuad();

	// Render blur
	renderBlur();

	// Bind the default frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, w, h);
	glClearColor(0.6, 0.0, 0.0, 0.1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render with postFxShader
	// Update time in PostFX Shader (required by the 'shrooms effect)
	glUseProgram(postFxShader);
	setUniformSlow(postFxShader, "frameBufferTexture", 0);
	setUniformSlow(postFxShader, "blurredFrameBufferTexture", 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, postProcessFBO.colorTextureTarget);
	glActiveTexture(GL_TEXTURE1);	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, verticalBlurFBO.colorTextureTarget);
	setUniformSlow(postFxShader, "time", currentTime);
	drawFullScreenQuad();

	// Copy a frame buffer to the default frame buffer
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//	glBindFramebuffer(GL_READ_FRAMEBUFFER, horizontalBlurFBO.id);
//	glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT,
//		GL_NEAREST);

	glUseProgram( 0 );	
	CHECK_GL_ERROR();

	CHECK_GL_ERROR();
	glutSwapBuffers();  // swap front and back buffer. This frame will now be displayed.
}

void handleKeys(unsigned char key, int /*x*/, int /*y*/)
{
	switch(key)
	{
	case 27:    /* ESC */
		exit(0); /* dirty exit */
		break;   /* unnecessary, I know */
	case 32:    /* space */
		paused = !paused;
		break;
	}
}



void handleSpecialKeys(int key, int /*x*/, int /*y*/)
{
	switch(key)
	{
	case GLUT_KEY_LEFT:
		printf("Left arrow\n");
		break;
	case GLUT_KEY_RIGHT:
		printf("Right arrow\n");
		break;
	case GLUT_KEY_UP:
	case GLUT_KEY_DOWN:
		break;
	}
}



void mouse(int button, int state, int x, int y)
{
	// reset the previous position, such that we only get movement performed after the button
	// was pressed.
	prev_x = x;
	prev_y = y;

	bool buttonDown = state == GLUT_DOWN;

	switch(button)
	{
	case GLUT_LEFT_BUTTON:
		leftDown = buttonDown;
		break;
	case GLUT_MIDDLE_BUTTON:
		middleDown = buttonDown;
		break;
	case GLUT_RIGHT_BUTTON: 
		rightDown = buttonDown;
	default:
		break;
	}
}

void motion(int x, int y)
{
	int delta_x = x - prev_x;
	int delta_y = y - prev_y;

	if(middleDown)
	{
		camera_r -= float(delta_y) * 0.3f;
		// make sure cameraDistance does not become too small
		camera_r = max(0.1f, camera_r);
	}
	if(leftDown)
	{
		camera_phi	-= float(delta_y) * 0.3f * float(M_PI) / 180.0f;
		camera_phi = min(max(0.01f, camera_phi), float(M_PI) - 0.01f);
		camera_theta += float(delta_x) * 0.3f * float(M_PI) / 180.0f;
	}

	if(rightDown)
	{
		camera_target_altitude += float(delta_y) * 0.1f; 
	}
	prev_x = x;
	prev_y = y;
}



void idle( void )
{
	static float startTime = float(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
	// Here is a good place to put application logic.
	if (!paused)
	{
		currentTime = float(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f - startTime;
	}

	// rotate light around X axis, sunlike fashion.
	// do one full revolution every 20 seconds.
	float4x4 rotateLight = make_rotation_x<float4x4>(2.0f * M_PI * currentTime / 20.0f);
	// rotate and update global light position.
	lightPosition = make_vector3(rotateLight * make_vector(30.1f, 450.0f, 0.1f, 1.0f));

	glutPostRedisplay();  
	// Uncommenting the line above tells glut that the window 
	// needs to be redisplayed again. This forces the display to be redrawn
	// over and over again. 
}

int main(int argc, char *argv[])
{
#	if defined(__linux__)
	linux_initialize_cwd();
#	endif // ! __linux__

	glutInit(&argc, argv);
	/* open window of size 800x600 with double buffering, RGB colors, and Z-buffering */
#	if defined(GLUT_SRGB)
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_SRGB | GLUT_DEPTH);
#	else // !GLUT_SRGB
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	printf( "Warning: GLUT_SRGB not supported by your GLUT!\n" );
#	endif // ! GLUT_SRGB
	glutInitWindowSize(800,512);
	glutCreateWindow("3D World Tutorial");
	glutKeyboardFunc(handleKeys);
	glutSpecialFunc(handleSpecialKeys);
	/* the display function is called once when the gluMainLoop is called,
	* but also each time the window has to be redrawn due to window 
	* changes (overlap, resize, etc). It can also be forced to be called
	* by making a glutPostRedisplay() call 
	*/
	glutDisplayFunc(display);	// This is the main redraw function
	glutMouseFunc(mouse);		// callback function on mouse buttons
	glutMotionFunc(motion);		// callback function on mouse movements
	glutIdleFunc( idle );

	glutDisplayFunc(display);	// Set the main redraw function

	initGL();

	glutMainLoop();  /* start the program main loop */

	return 0;          
}

void createShadowMap(int width, int height)
{
	glGenTextures(1, &shadowMapTexture);
	glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32,
				width, height, 0,
				GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	float4 ones = {1.0, 1.0, 1.0, 1.0};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &ones.x);

	glGenFramebuffers(1, &shadowMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							GL_TEXTURE_2D, shadowMapTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	// Activate the default framebuffer again
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(shaderProgram);
	setUniformSlow(shaderProgram, "shadowMap", 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
}

void drawFullScreenQuad()
{
	static GLuint vertexArrayObject = 0; 
	static int nofVertices = 4; 

  // do this initialization first time the function is called... somewhat dodgy, but works for demonstration purposes
	if (vertexArrayObject == 0)
	{
		glGenVertexArrays(1, &vertexArrayObject); 
    static const float2 positions[] = {
      {-1.0f, -1.0f},
      { 1.0f, -1.0f},
      { 1.0f,  1.0f},
      {-1.0f,  1.0f},
    };
		createAddAttribBuffer(vertexArrayObject, positions, sizeof(positions), 0, 2, GL_FLOAT);
	}

	glBindVertexArray(vertexArrayObject); 
	glDrawArrays(GL_QUADS, 0, nofVertices); 
}

FBOInfo createPostProcessFBO(int width, int height)
{
	FBOInfo fbo;
	
	fbo.width = width;
	fbo.height = height;

	glGenTextures(1, &fbo.colorTextureTarget);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fbo.colorTextureTarget);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &fbo.id);
	// Bind the framebuffer such that following commands will affect it
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							GL_TEXTURE_RECTANGLE_ARB, fbo.colorTextureTarget, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glGenRenderbuffers(1, &fbo.depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, fbo.depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	// Associate our created depth buffer with the FBO
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								GL_RENDERBUFFER, fbo.depthBuffer);

	GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		fatal_error("Framebuffer not complete");
	}
	
	return fbo;
}

void renderBlur()
{
	// Bind the horizontal blur frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, horizontalBlurFBO.id);
	glClearColor(0.0,0.0,0.0,1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// Render with horizontalBlurShader
	glUseProgram(horizontalBlurShader);
	setUniformSlow(horizontalBlurShader, "frameBufferTexture", 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, cutoffFBO.colorTextureTarget);
	drawFullScreenQuad();

	// Bind the vertical blur frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, verticalBlurFBO.id);
	glClearColor(0.0,0.0,0.0,1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render with verticalBlurShader
	glUseProgram(verticalBlurShader);
	setUniformSlow(verticalBlurShader, "frameBufferTexture", 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, horizontalBlurFBO.colorTextureTarget);
	drawFullScreenQuad();
}