#define _USE_MATH_DEFINES
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <string.h>
#include <iostream>
#include <math.h>
#include <time.h>
#include <string>
#include <vector> // Standard template library class
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <omp.h>
//glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/half_float.hpp>
//imgui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaders.h"

// shader program ID
GLuint shaderProgram;
GLfloat ftime = 0.0f;

GLint modelParameter; // modeling matrix
GLint viewParameter; // viewing matrix
GLint projParameter; // projection matrix

GLFWwindow* window;
// the main window size
GLint wWindow = 800;
GLint hWindow = 600;

/*********************************
Some OpenGL-related functions
**********************************/

// called when a window is reshaped
void Reshape(GLFWwindow* window, int w, int h)
{
	glViewport(0, 0, w, h);
	// remember the settings for the camera
	wWindow = w;
	hWindow = h;
}

void DrawGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// the main rendering function
void RenderObjects()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glColor3f(0, 0, 0);
	glPointSize(2);
	glLineWidth(1);

	// set the projection and view once for the scene
	glm::mat4 view = glm::mat4(1.0);
	glm::mat4 proj = glm::perspective(80.0f,// fovy
		1.0f, // aspect
		0.01f, 1000.f); // near, far
	glUniformMatrix4fv(projParameter, 1, GL_FALSE, glm::value_ptr(proj));
	view = glm::lookAt(glm::vec3(10.0f, 5.0f, 10.0f),// eye
		glm::vec3(0.0f, 0.0f, 0.0f),  // destination
		glm::vec3(0.0f, 1.0f, 0.0f)); // up

	glUniformMatrix4fv(viewParameter, 1, GL_FALSE, glm::value_ptr(view));

	DrawGui();
}

void Idle(void)
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ftime += 0.05;
	glUseProgram(shaderProgram);
	RenderObjects();
	glfwSwapBuffers(window);
}

void Display(void)
{

}

// keyboard callback
void Kbd(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		}
	}
}

void InitializeProgram(GLuint* program)
{
	std::vector<GLuint> shaderList;

	// load and compile shaders 	
	shaderList.push_back(CreateShader(GL_VERTEX_SHADER, LoadShader("shaders/phong.vert")));
	shaderList.push_back(CreateShader(GL_FRAGMENT_SHADER, LoadShader("shaders/phong.frag")));

	// create the shader program and attach the shaders to it
	*program = CreateProgram(shaderList);

	// delete shaders (they are on the GPU now)
	std::for_each(shaderList.begin(), shaderList.end(), glDeleteShader);

	modelParameter = glGetUniformLocation(*program, "model");
	viewParameter = glGetUniformLocation(*program, "view");
	projParameter = glGetUniformLocation(*program, "proj");
}

int InitializeGL(GLFWwindow*& window)
{
	if (!glfwInit())
		return -1;
	window = glfwCreateWindow(wWindow, hWindow, "Path Tracing", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	glfwSetKeyCallback(window, Kbd);
	glfwSetFramebufferSizeCallback(window, Reshape);
	glfwMakeContextCurrent(window);
	glewInit();
	InitializeProgram(&shaderProgram);
	glEnable(GL_DEPTH_TEST);

	return 0;
}

int main(int argc, char** argv)
{
	int initRes = InitializeGL(window);
	if (initRes)
		return initRes;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	while (!glfwWindowShouldClose(window))
	{
		Idle();
		Display();
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
