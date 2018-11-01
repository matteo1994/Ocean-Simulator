 #pragma once

#ifndef __PI__
#define __PI__ 
#define PI 3.14159265358979323846264338327950288
#endif __PI__


#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "stdafx.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <GLFW/glfw3.h>

class Controls {
private:
	// position
	glm::vec3 position;
	// horizontal angle : toward -Z
	float horizontalAngle;
	// vertical angle : 0, look at the horizon
	float verticalAngle;

	float maxVerticalAngle = 0.05f;
	// Initial Field of View
	float initialFoV;
	float speed;
	float mouseSpeed;

	glm::vec3 direction;
	glm::vec3 up;
	glm::vec3 right;

	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;

	static double yScroll;

	double currentFoV;

	double lastTime;
public:
	Controls();
	Controls(glm::vec3 pos, float hor, float ver, float FoV, float mov_speed, float mouse_speed)
		: position{ pos }, horizontalAngle{ hor }, verticalAngle{ ver }, currentFoV {FoV},
		initialFoV{ FoV }, speed{ mov_speed }, mouseSpeed{ mouse_speed }, lastTime{ glfwGetTime() } {};

	void computeMatrixFromInputs();
	glm::mat4 getProjectionMatrix() { return projectionMatrix; };
	glm::mat4 getViewMatrix() { return viewMatrix; };

	glm::vec3 getPosition() { return position; };
	float getInitialFoV() { return initialFoV; };
	
	static void setyScroll(double y) { Controls::yScroll = y; };

};


#endif __CONTROLS_H__ 