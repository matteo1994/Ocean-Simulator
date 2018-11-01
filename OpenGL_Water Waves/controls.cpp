 #pragma once

#include "controls.h"

double Controls::yScroll = 0;

Controls::Controls() {};

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Controls::setyScroll(yoffset);
}

void Controls::computeMatrixFromInputs() {

	//DeltaTime calculation

	double currentTime = glfwGetTime();
	float deltaTime = float(currentTime - this->lastTime);
	

	//Orientation phase

	GLFWwindow* window = glfwGetCurrentContext();

	glfwSetScrollCallback(window, scroll_callback);


	int xSize, ySize;
	glfwGetWindowSize(window, &xSize, &ySize);


	// Get mouse position
	double xPos, yPos;
	glfwGetCursorPos(window, &xPos, &yPos);


	//glfwSetCursorPos(window, xSize / 2, ySize / 2);

	// Compute new orientation
	this->horizontalAngle += mouseSpeed * deltaTime * float(xSize / 2 - xPos);
	this->verticalAngle += mouseSpeed * deltaTime * float(ySize / 2 - yPos);

	this->verticalAngle = (this->verticalAngle > 0.3f) ? 0.3f : this->verticalAngle;
	this->verticalAngle = (this->verticalAngle < -0.3f) ? -0.3f : this->verticalAngle;

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	this->direction = glm::vec3(
		cos(this->verticalAngle) * sin(this->horizontalAngle),
		sin(this->verticalAngle),
		cos(this->verticalAngle) * cos(this->horizontalAngle)
	);

	// Right vector
	this->right = glm::vec3(
		sin(this->horizontalAngle - PI / 2.0f),
		0,
		cos(this->horizontalAngle - PI / 2.0f)
	);

	// Up vector : perpendicular to both direction and right
	this->up = glm::cross(this->right, this->direction);


	//Position

	// Move forward
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		position += direction * deltaTime * speed;
	}
	// Move backward
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		position -= direction * deltaTime * speed;
	}
	// Strafe right
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		position += right * deltaTime * speed;
	}
	// Strafe left
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		position -= right * deltaTime * speed;
	}

	currentFoV -= 5 * Controls::yScroll;

	this->projectionMatrix = glm::perspective(glm::radians((float)currentFoV), (xSize + 0.0f) / ySize, 0.1f, 200.0f);

	this->viewMatrix = glm::lookAt(
		position,           // Camera is here
		position + direction, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);	

	this->lastTime = currentTime;
	Controls::yScroll = 0;
}



