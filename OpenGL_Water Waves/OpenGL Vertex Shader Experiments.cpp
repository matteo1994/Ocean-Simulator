

#include "stdafx.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <GLFW/glfw3.h>
#include "LoadShaders.h"
#include "controls.h"
#include "Utilities.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

//Macros for textures
#define IRRADIANCE_UNIT 0
#define TRANSMITTANCE_UNIT 1
#define SAND 2

#define BUFFER_OFFSET(a) ((void*)(a))


//Macros for waves generation
#define srnd() (2*frandom(&seed) - 1)
#define nbAngles 5 // even
#define angle(i) (1.5*(((i)%nbAngles)/(float)(nbAngles/2)-1 ))
#define dangle() (1.5/(float)(nbAngles/2))

enum VAO_IDs { Grid, NumVAOs };
enum Buffer_IDs { VBO, VBO_i, NumBuffers };
enum Shader_IDs { envToCube, Cube, Irradiance, Ocean, NumShaders};
enum Attrib_IDs { vPosition = 0 };
enum Texture_IDs {HDR_Image, IrradianceMap, CubeMap, Skydome, TrasmittanceMap, RampIrradiance, NumTextures};
enum FrameBuffer_IDs {EnvMap, IrrMap, NumFB};
int vboSize = NumBuffers;
GLuint VAOs[NumVAOs];
GLuint Buffers[NumBuffers];
GLuint Shaders[NumShaders];
GLuint Textures[NumTextures];
GLuint MatrixID;
GLuint cubemapID;
Controls controls;
glm::mat4 mvp;
GLuint NumVertices;
GLuint NumIndices;


//Screen info
int xSize, ySize;

// position
glm::vec3 position = glm::vec3(0, 0, 5);
double lastTime;
int nbFrames;

//Camera Parameters
float cameraTheta = 0;
float cameraPhi = PI / 3;
float cameraHeight = 10.0;


//Env Parameters
float sampleSphere = 0.025;
float sunTheta = PI / 3;
float sunPhi = - PI * 4/6 + 0.075;
float sunLuminance = 1.0f;
float hdrExposure = 0.4;


//Grid Parameters
int gridSize = 8;
float nyquistMin = 1.0;
float nyquistMax = 1.5;
glm::vec4 vboParams;
bool fill = true;
bool normals = false;
bool reflactance = false;
bool irradiance = true;


// WAVES PARAMETERS (INPUT)

GLuint waveTex;

const int nbWaves = 60;

glm::vec4 waves[nbWaves];
float omegas[nbWaves];
float amplitudes[nbWaves];
float kx[nbWaves];
float ky[nbWaves];

float lambdaMin = 0.02;

float lambdaMax = 30.0;

float heightMax = 0.32;

float waveDirection = 2.4;

float U0 = 10.0;

float waveDispersion = 1.25f;

// WAVE STATISTICS (OUTPUT)

float sigmaXsq = 0.0;

float sigmaYsq = 0.0;

float meanHeight = 0.0;

float heightVariance = 0.0;

float amplitudeMax = 0.0;

void renderCube();

//Generate Mesh

void generateMesh()
{
	
	glBindVertexArray(VAOs[Grid]);
	glCheckError();

	if (vboSize != 0) {
		glDeleteBuffers(1, &Buffers[VBO]);
		glDeleteBuffers(1, &Buffers[VBO_i]);
	}
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VBO]);

	float horizon = tan(cameraTheta);
	float s = std::fmin(1.5f, 0.5f + horizon * 0.6f);
	float vmargin = 0.12;
	float hmargin = 0.12;

	int width, height;
	glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);

	vboParams = glm::vec4(width, height, gridSize, cameraTheta);
	glm::vec4 *data = new glm::vec4[int(ceil(height * (s + vmargin) / gridSize) + 5) * int(ceil(width * (1.0 + 2.0 * hmargin) / gridSize) + 5)];

	int n = 0;
	int nx = 0;
	for (float j = height * s - 0.1; j > -height * vmargin - gridSize; j -= gridSize) {
		nx = 0;
		for (float i = -width * hmargin; i < width * (1.0 + hmargin) + gridSize; i += gridSize) {
			data[n++] = glm::vec4(-1.0 + 2.0 * i / width, -1.0 + 2.0 * j / height, 0.0, 1.0);
			nx++;
		}
	}

	glBufferData(GL_ARRAY_BUFFER, n * 16, data, GL_STATIC_DRAW);
	delete[] data;

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VBO]);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glCheckError();

	glBindBuffer(GL_ARRAY_BUFFER, Buffers[VBO_i]);

	vboSize = 0;
	GLuint *indices = new GLuint[6 * int(ceil(height * (s + vmargin) / gridSize) + 4) * int(ceil(width * (1.0 + 2.0 * hmargin) / gridSize) + 4)];

	int nj = 0;
	for (float j = height * s - 0.1; j > -height * vmargin; j -= gridSize) {
		int ni = 0;
		for (float i = -width * hmargin; i < width * (1.0 + hmargin); i += gridSize) {
			indices[vboSize++] = ni + (nj + 1) * nx;
			indices[vboSize++] = (ni + 1) + (nj + 1) * nx;
			indices[vboSize++] = (ni + 1) + nj * nx;
			indices[vboSize++] = (ni + 1) + nj * nx;
			indices[vboSize++] = ni + (nj + 1) * nx;
			indices[vboSize++] = ni + nj * nx;
			ni++;
		}
		nj++;
	}

	glBufferData(GL_ARRAY_BUFFER, vboSize * 4, indices, GL_STATIC_DRAW);
	delete[] indices;
}

// ----------------------------------------------------------------------------
// WAVES GENERATION
// ----------------------------------------------------------------------------


//Functions to generate random numbers
long lrandom(long *seed)
{
	*seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
	return *seed;
}

float frandom(long *seed)
{
	long r = lrandom(seed) >> (31 - 24);
	return r / (float)(1 << 24);
}

//Function to generate gaussian random numbers
inline float grandom(float mean, float stdDeviation, long *seed)
{
	float x1, x2, w, y1;
	static float y2;
	static int use_last = 0;

	if (use_last) {
		y1 = y2;
		use_last = 0;
	}
	else {
		do {
			x1 = 2.0f * frandom(seed) - 1.0f;
			x2 = 2.0f * frandom(seed) - 1.0f;
			w = x1 * x1 + x2 * x2;
		} while (w >= 1.0f);
		w = sqrt((-2.0f * log(w)) / w);
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}
	return mean + y1 * stdDeviation;
}



void generateWaves()
{
	long seed = 1234567;
	float min = log(lambdaMin) / log(2.0f);
	float max = log(lambdaMax) / log(2.0f);

	sigmaXsq = 0.0;
	sigmaYsq = 0.0;
	meanHeight = 0.0;
	heightVariance = 0.0;
	amplitudeMax = 0.0;

	/*
	if (waves != NULL) {
		delete[] waves;
	}
	waves = new glm::vec4[nbWaves];
	*/

	float Wa[nbAngles]; // normalised gaussian samples
	int index[nbAngles]; // to hash angle order
	float s = 0;
	for (int i = 0; i < nbAngles; i++) {
		index[i] = i;
		float a = angle(i);
		s += Wa[i] = exp(-0.5*a*a);
	}
	for (int i = 0; i < nbAngles; i++) {
		Wa[i] /= s;
	}

	for (int i = 0; i < nbWaves; ++i) {
		float x = i / (nbWaves - 1.0f);

		//Find a wavelength in the range [Lambda(min), Lambda(max)] according to the wave index
		float lambda = pow(2.0f, (1.0f - x) * min + x * max);

		float ktheta = grandom(0.0f, 1.0f, &seed) * waveDispersion;

		//Calculate K (wavenumber)
		float knorm = 2.0f * PI / lambda;
		//Angular frequency
		float omega = sqrt(9.81f * knorm);
		float amplitude; 

		//Fractional part of the range [Lambda(min), Lambda(max)]
		float step = (max - min) / (nbWaves - 1); // dlambda/di

		//w0 = gravity / Wind'speed at 20m 
		float omega0 = G / U0;

		if ((i % (nbAngles)) == 0) { // scramble angle ordre
			for (int k = 0; k < nbAngles; k++) {   // do N swap in indices
				int n1 = lrandom(&seed) % nbAngles, n2 = lrandom(&seed) % nbAngles, n;
				n = index[n1];
				index[n1] = index[n2];
				index[n2] = n;
			}
		}

		ktheta = waveDispersion * (angle(index[(i) % nbAngles]) + 0.4*srnd()*dangle());
		ktheta *= 1.0 / (1.0 + 40.0*pow(omega0 / omega, 4));

		//Calculate the amplitude according to the energy distribution of gravity waves as a function of their frequency
		amplitude = (8.1e-3*G*G) / pow(omega, 5) * exp(-0.74*pow(omega0 / omega, 4));
		amplitude *= 0.5*sqrt(2 * PI * G / lambda) * nbAngles * step;
		amplitude = 3 * heightMax*sqrt(amplitude);

		if (amplitude > 1.0f / knorm) {
			amplitude = 1.0f / knorm;
		}
		else if (amplitude < -1.0f / knorm) {
			amplitude = -1.0f / knorm;
		}

		//waves[i].x = amplitude;
		amplitudes[i] = amplitude;
		//waves[i].y = omega;
		omegas[i] = omega;
		//waves[i].z = knorm * cos(ktheta);
		kx[i] = knorm * cos(ktheta);
		//waves[i].w = knorm * sin(ktheta);
		ky[i] = knorm * sin(ktheta);
		sigmaXsq += pow(cos(ktheta), 2.0f) * (1.0 - sqrt(1.0 - knorm * knorm * amplitude * amplitude));
		sigmaYsq += pow(sin(ktheta), 2.0f) * (1.0 - sqrt(1.0 - knorm * knorm * amplitude * amplitude));
		meanHeight -= knorm * amplitude * amplitude * 0.5f;
		heightVariance += amplitude * amplitude * (2.0f - knorm * knorm * amplitude * amplitude) * 0.25f;
		amplitudeMax += fabs(amplitude);
	}

	float var = 4.0f;
	amplitudeMax = 2.0f * var * sqrt(heightVariance);

}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
		cameraTheta = std::fmin(cameraTheta + 5.0f / 180.0f * PI, PI / 2.0f - 0.001f);
	}
	else if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
		cameraTheta = std::fmax(cameraTheta - 5.0f / 180.0 * PI, -PI / 4.0f + 0.0001f);
	}
	else if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
		fill = !fill;
	}
	else if (key == GLFW_KEY_N && action == GLFW_PRESS) {
		normals = !normals;
	}
	else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		reflactance = !reflactance;
	}
	else if (key == GLFW_KEY_I && action == GLFW_PRESS) {
		irradiance = !irradiance;

	}
	else if (key == GLFW_KEY_W && action == GLFW_PRESS) {
		U0 += 0.2;
		generateWaves();
	}
	else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
		U0 = std::fmax(1.0, U0 - 0.2);
		generateWaves();
	}
	else if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
		cameraPhi = std::fmax(cameraPhi - 5.0 / 180 * PI, -PI / 2 + 0.001f);
	}
	else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
		cameraPhi = std::fmin(cameraPhi + 5.0 / 180 * PI,  PI / 2 - 0.0001);
	}
	else if (key == GLFW_KEY_A && action == GLFW_PRESS) {
		waveDirection = std::fmax(0.0, waveDirection - 0.1);
	}
	else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
		waveDirection = std::fmin(6.3, waveDirection + 0.1);
	}

}


void
init(void)
{
	// Read our .obj file
	//std::vector< glm::vec3 > vertices;
	//std::vector< glm::vec2 > uvs;
	//std::vector< glm::vec3 > normals;
	//bool res = loadOBJ("./floor.obj", vertices, uvs, normals);

	glGenVertexArrays(NumVAOs, VAOs);
	glGenBuffers(NumBuffers, Buffers);
	glGenTextures(NumTextures, Textures);

	/* ---  LOAD CUBE MAP --- */

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL); // set depth function to less than AND equal for skybox depth trick.

	ShaderInfo shaders[] = {
		{ GL_VERTEX_SHADER, "equirectangular.vs.glsl" },
	{ GL_FRAGMENT_SHADER, "equirectangular.fs.glsl" },
	{ GL_NONE, NULL }
	};
	Shaders[envToCube] = LoadShaders(shaders);

	ShaderInfo shaders2[] = {
		{ GL_VERTEX_SHADER, "skybox.vs.glsl" },
	{ GL_FRAGMENT_SHADER, "skybox.fs.glsl" },
	{ GL_NONE, NULL }
	};
	Shaders[Cube] = LoadShaders(shaders2);

	ShaderInfo shaders3[] = {
		{ GL_VERTEX_SHADER, "skybox.vs.glsl" },
	{ GL_FRAGMENT_SHADER, "irradiance.fs.glsl" },
	{ GL_NONE, NULL }
	};
	Shaders[Irradiance] = LoadShaders(shaders3);

	glUseProgram(Shaders[Cube]);
	glCheckError();

	GLuint id = glGetUniformLocation(Shaders[Cube], "environmentMap");
	glCheckError();
	glUniform1i(id, CubeMap);
	glCheckError();

	GLuint captureFBO[NumFB];
	GLuint captureRBO[NumFB];

	glGenFramebuffers(NumFB, captureFBO);
	glGenRenderbuffers(NumFB, captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO[EnvMap]);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO[EnvMap]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO[EnvMap]);

	stbi_set_flip_vertically_on_load(true);
	int w, h, nrComponents;
	float * hdr_image = stbi_loadf("GCanyon_C_YumaPoint_3k.hdr", &w, &h, &nrComponents, 0);

	if (hdr_image)
	{
		glActiveTexture(GL_TEXTURE0 + HDR_Image);
		glBindTexture(GL_TEXTURE_2D, Textures[HDR_Image]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, hdr_image);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(hdr_image);
	}
	else
	{
		std::cout << "Failed to load HDR image." << std::endl;
	}

	glActiveTexture(GL_TEXTURE0 + CubeMap);

	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[CubeMap]);
	for (unsigned int i = 0; i < 6; ++i)
	{
		// note that we store each face with 16 bit floating point values
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
			512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// convert HDR equirectangular environment map to cubemap equivalent
	glUseProgram(Shaders[envToCube]);
	glCheckError();

	glUniform1i(glGetUniformLocation(Shaders[envToCube], "equirectangularMap"), HDR_Image);
	glCheckError();
	glUniformMatrix4fv(glGetUniformLocation(Shaders[envToCube], "Proj"), 1, GL_FALSE, &captureProjection[0][0]);
	glCheckError();

	glActiveTexture(GL_TEXTURE0 + HDR_Image);
	glBindTexture(GL_TEXTURE_2D, Textures[HDR_Image]);

	glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO[EnvMap]);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(glGetUniformLocation(Shaders[envToCube], "View"), 1, GL_FALSE, &captureViews[i][0][0]);
		glCheckError();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, Textures[CubeMap], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube(); // renders a 1x1 cube
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	
	//Irradiance Map from the EnvMap
	glActiveTexture(GL_TEXTURE0 + IrradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[IrradianceMap]);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0,
			GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO[IrrMap]);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO[IrrMap]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	glUseProgram(Shaders[Irradiance]);
	glCheckError();

	glUniform1f(glGetUniformLocation(Shaders[Irradiance], "sampleDelta"), sampleSphere);
	glUniform1i(glGetUniformLocation(Shaders[Irradiance], "environmentMap"), CubeMap);
	glUniform1i(glGetUniformLocation(Shaders[Irradiance], "noModel"), true);
	glUniformMatrix4fv(glGetUniformLocation(Shaders[Irradiance], "Proj"), 1, GL_FALSE, &captureProjection[0][0]);
	glCheckError();
	
	glActiveTexture(GL_TEXTURE0 + CubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[CubeMap]);
	
	glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO[IrrMap]);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(glGetUniformLocation(Shaders[Irradiance], "View"), 1, GL_FALSE, &captureViews[i][0][0]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, Textures[IrradianceMap], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	

	// then before rendering, configure the viewport to the original framebuffer's screen dimensions
	int scrWidth, scrHeight;
	glfwGetFramebufferSize(glfwGetCurrentContext(), &scrWidth, &scrHeight);
	glViewport(0, 0, scrWidth, scrHeight);

	 
	glfwGetWindowSize(glfwGetCurrentContext(), &xSize, &ySize);

	ShaderInfo shaders4[] = {
		{ GL_VERTEX_SHADER, "waves2.vs.glsl" },
	{ GL_FRAGMENT_SHADER, "waves.fs.glsl" },
	{ GL_NONE, NULL }
	};
	Shaders[Ocean] = LoadShaders(shaders4);


	unsigned char * sky_image = stbi_load("map/skydome_Fisheye4_minus2.png", &w, &h, &nrComponents, 0);
	 
	if (sky_image)
	{
		glActiveTexture(GL_TEXTURE0 + Skydome);
		glBindTexture(GL_TEXTURE_2D, Textures[Skydome]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, sky_image);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(sky_image);
	}
	else
	{
		std::cout << "Failed to load skydome image." << std::endl;
	}

	float * trasmittanceData = new float[256 * 64 * 3];
	FILE * f;
	errno_t err;
	if ((err = fopen_s(&f, "transmittance.raw", "rb")) != 0) { printf("Image could not be opened\n");}
	fread_s(trasmittanceData, 256 * 64 * 3 * sizeof(float), 1, 256 * 64 * 3 * sizeof(float), f);
	fclose(f);
	glActiveTexture(GL_TEXTURE0 + TrasmittanceMap);
	glBindTexture(GL_TEXTURE_2D, Textures[TrasmittanceMap]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB, 256, 64, 0, GL_RGB, GL_FLOAT, trasmittanceData);
	delete[] trasmittanceData;

	FILE * ff;
	float *irrdata = new float[16 * 64 * 3];
	if ((err = fopen_s(&ff, "irradiance.raw", "rb")) != 0) { printf("Image could not be opened\n"); }
	fread_s(irrdata, 16 * 64 * 3 * sizeof(float), 1, 16 * 64 * 3 * sizeof(float), ff);
	fclose(ff);
	glActiveTexture(GL_TEXTURE0 + RampIrradiance);
	glBindTexture(GL_TEXTURE_2D, Textures[RampIrradiance]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB, 64, 16, 0, GL_RGB, GL_FLOAT, irrdata);
	delete[] irrdata;

	glUseProgram(Shaders[Ocean]);
	glCheckError();

	glUniform1i(glGetUniformLocation(Shaders[Ocean], "transmittanceSampler"), TrasmittanceMap);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "skyIrradianceSampler"), RampIrradiance);
		
	generateWaves();

	generateMesh();

	lastTime = glfwGetTime();
}

 

//---------------------------------------------------------------------
//
// display
//
void
display(void)
{
	//glEnable(GL_CULL_FACE);

	glfwGetWindowSize(glfwGetCurrentContext(), &xSize, &ySize);

	
	if (vboParams.x != xSize || vboParams.y != ySize || vboParams.z != gridSize || vboParams.w != cameraTheta) {
		generateMesh();
	}

	// Measure speed
	double currentTime = glfwGetTime();


	if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_PAGE_UP) == GLFW_PRESS) {
		float tmp = cameraHeight + (10 * 0.1f * (currentTime - lastTime));
		cameraHeight = std::fmin(200.0f, tmp);
	}
	else if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) {
		float tmp = cameraHeight - (10 * 0.1f * (currentTime - lastTime));
		cameraHeight = std::fmax(1.5f, tmp);
	}


	nbFrames++;
	if (currentTime - lastTime >= 1.0) { // If last prinf() was more than 1 sec ago
										 // printf and reset timer
		printf("%f ms/frame  cameraT=%f cameraH=%f sunT=%f sunP=%f\n", 1000.0 / double(nbFrames), cameraTheta * 180.0f / PI, cameraHeight, sunTheta * 180.0f / PI, sunPhi * 180.0f / PI);
		nbFrames = 0;
		lastTime += 1.0;
	}

	float ch = cameraHeight - meanHeight;

	glm::mat4 view = 
	glm::mat4(
		glm::vec4(0.0, 0.0, -1.0, 0.0), 
		glm::vec4(-1.0, 0.0, 0.0, 0.0), 
		glm::vec4(0.0, 1.0, 0.0, 0.0), 
		glm::vec4(0.0, -ch, 0.0, 1.0)
	);

	glm::mat4 rotationXMat =
		glm::mat4(
			glm::vec4(1.0, 0.0, 0.0, 0.0),
			glm::vec4(0.0, cos(cameraTheta), sin(cameraTheta), 0.0),
			glm::vec4(0.0, -sin(cameraTheta), cos(cameraTheta), 0.0),
			glm::vec4(0.0, 0.0, 0.0, 1.0)
		);

	glm::mat4 rotationYMat =
		glm::mat4(
			glm::vec4(cos(cameraPhi), 0.0, -sin(cameraPhi), 0.0),
			glm::vec4(0.0, 1.0, 0.0, 0.0),
			glm::vec4(sin(cameraPhi), 0.0, cos(cameraPhi), 0.0),
			glm::vec4(0.0, 0.0, 0.0, 1.0)
		);

	view = rotationYMat * view;
	view = rotationXMat * view;

	//mat4f proj = mat4f::perspectiveProjection(90.0, float(width) / float(height), 0.1 * ch, 1000000.0 * ch);
	glm::mat4 proj = glm::perspective(glm::radians((float)90.0), (xSize + 0.0f) / ySize, 0.1f*ch, 1000000.0f * ch);

	float worldToWind[4];
	worldToWind[0] = cos(waveDirection);
	worldToWind[1] = sin(waveDirection);
	worldToWind[2] = -sin(waveDirection);
	worldToWind[3] = cos(waveDirection);

	float windToWorld[4];
	windToWorld[0] = cos(waveDirection);
	windToWorld[1] = -sin(waveDirection);
	windToWorld[2] = sin(waveDirection);
	windToWorld[3] = cos(waveDirection);

	//glm::mat4 Projection = controls.getProjectionMatrix();
	glm::mat4 Projection = proj;

	// Camera matrix
	//glm::mat4 View = controls.getViewMatrix();

	glm::mat4 View = view;

	// Model matrix : an identity matrix (model will be at the origin)
	glm::mat4 ModelM = glm::mat4(1.0f);

	glm::mat4 mvp = Projection * View * ModelM; // Remember, matrix multiplication is the other way around

	glm::mat4 screenToCamera = glm::inverse(Projection);
	glm::mat4 cameraToWorld = glm::inverse(View);													   
	glm::mat4 screenToWorld = glm::inverse(mvp);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glPolygonMode(GL_FRONT, (fill) ? GL_FILL : GL_LINE);
	glPolygonMode(GL_BACK, (fill) ? GL_FILL : GL_LINE);
	
	// Activate simple shading program
	glUseProgram(Shaders[Ocean]);
	glCheckError();

	// Get a handle for our "MVP" uniform
	// Only during the initialisation
	MatrixID = glGetUniformLocation(Shaders[Ocean], "MVP");
	glCheckError();

	// Send our transformation to the currently bound shader, in the "MVP" uniform
	// This is done in the main loop since each model will have a different MVP matrix (At least for the M part)
	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &mvp[0][0]);
	glCheckError();

	glm::vec4 sun = glm::vec4(sin(sunTheta) * cos(sunPhi), sin(sunTheta) * sin(sunPhi), cos(sunTheta), 0.0);

	glUniformMatrix4fv(glGetUniformLocation(Shaders[Ocean], "screenToCamera"), 1, false, &screenToCamera[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(Shaders[Ocean], "cameraToWorld"), 1, false, &cameraToWorld[0][0]);
	glUniformMatrix2fv(glGetUniformLocation(Shaders[Ocean], "worldToWind"), 1,true, worldToWind);
	glUniformMatrix2fv(glGetUniformLocation(Shaders[Ocean], "windToWorld"), 1, true, windToWorld);
	glUniform3f(glGetUniformLocation(Shaders[Ocean], "worldCamera"), 0.0, 0.0, ch);
	glUniform3f(glGetUniformLocation(Shaders[Ocean], "worldCamera"), 0.0, 0.0, ch);
	glUniform3f(glGetUniformLocation(Shaders[Ocean], "worldSunDir"), sun.x, sun.y, sun.z);

	glUniform1f(glGetUniformLocation(Shaders[Ocean], "nbWaves"), nbWaves);
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "heightOffset"), -meanHeight);
	glUniform2f(glGetUniformLocation(Shaders[Ocean], "sigmaSqTotal"), sigmaXsq, sigmaYsq);
	
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "time"), currentTime);
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "hdrExposure"), hdrExposure);
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "sunLuminance"), sunLuminance);

	glUniform1fv(glGetUniformLocation(Shaders[Ocean], "h"), nbWaves, &amplitudes[0]);
	glUniform1fv(glGetUniformLocation(Shaders[Ocean], "omega"), nbWaves, &omegas[0]);
	glUniform1fv(glGetUniformLocation(Shaders[Ocean], "kx"), nbWaves, &kx[0]);
	glUniform1fv(glGetUniformLocation(Shaders[Ocean], "ky"), nbWaves, &ky[0]);
	
	
	glUniform4f(glGetUniformLocation(Shaders[Ocean], "lods"),
		gridSize,
		atan(2.0 / ySize) * gridSize, // angle under which a screen pixel is viewed from the camera * gridSize
		log(lambdaMin) / log(2.0f),
		(nbWaves - 1.0f) / (log(lambdaMax) / log(2.0f) - log(lambdaMin) / log(2.0f)));
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "nyquistMin"), nyquistMin);
	glUniform1f(glGetUniformLocation(Shaders[Ocean], "nyquistMax"), nyquistMax);
	
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "irradianceMap"), IrradianceMap);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "radianceMap"), CubeMap);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "skyDome"), Skydome);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "NormalView"), normals);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "reflactance"), reflactance);
	glUniform1i(glGetUniformLocation(Shaders[Ocean], "Irradiance"), irradiance);

	glActiveTexture(GL_TEXTURE0 + IrradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[IrradianceMap]);
	glActiveTexture(GL_TEXTURE0 + CubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[CubeMap]);

	glActiveTexture(GL_TEXTURE0 + Skydome);
	glBindTexture(GL_TEXTURE_2D, Textures[Skydome]);
	glActiveTexture(GL_TEXTURE0 + TrasmittanceMap);
	glBindTexture(GL_TEXTURE_2D, Textures[TrasmittanceMap]);
	glActiveTexture(GL_TEXTURE0 + RampIrradiance);
	glBindTexture(GL_TEXTURE_2D, Textures[RampIrradiance]);
	
	glBindVertexArray(VAOs[Grid]);
	glCheckError();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffers[VBO_i]);
	glCheckError();
	

	// Draw the triangles !
	glDrawElements(
		GL_TRIANGLES,      // mode
		vboSize,	   // count
		GL_UNSIGNED_INT,   // type
		(void*)0           // element array buffer offset
	);
	glBindVertexArray(0);
	
	//Draw skybox

	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_FILL);

	glm::mat4 myTranslationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	glm::mat4 myScaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

	//Creates an identity quaternion(no rotation)
	glm::quat myQuaternion = glm::quat(0, 0, 0, 1);
	glm::vec3 EulerAngles = glm::vec3(PI / 2, 0.0f, 0.0f);
	myQuaternion = glm::quat(EulerAngles);


	glm::mat4 myRotationMatrix = glm::mat4_cast(myQuaternion);


	glm::mat4 myModelMatrix = myTranslationMatrix * myRotationMatrix * myScaleMatrix;

	glUseProgram(Shaders[Cube]);
	glCheckError();

	glUniformMatrix4fv(glGetUniformLocation(Shaders[Cube], "model"), 1, GL_FALSE, &myModelMatrix[0][0]);
	glCheckError();

	glUniformMatrix4fv(glGetUniformLocation(Shaders[Cube], "Proj"), 1, GL_FALSE, &Projection[0][0]);
	glCheckError();

	glUniformMatrix4fv(glGetUniformLocation(Shaders[Cube], "View"), 1, GL_FALSE, &View[0][0]);
	glCheckError();

	glUniform1i(glGetUniformLocation(Shaders[Cube], "environmentMap"), CubeMap);

	glActiveTexture(GL_TEXTURE0 + CubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Textures[CubeMap]);
	renderCube();


	glFlush();

}

int main() {

	glfwInit();

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Cube and Triangle  ", NULL, NULL);

	glfwMakeContextCurrent(window);
	glewInit();

	init();

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	do {
		display();

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);


	return 0;
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left

			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left

			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right

			// right face
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     

			// bottom face
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right

			// top face
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
