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
// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/half_float.hpp>
// imgui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaders.h"
#include "pathtracer.h"

GLuint shaderProgram;

GLFWwindow* window;
GLint wWindow = 800;
GLint hWindow = 600;

GLuint quadVao = -1;
GLuint frameTex = -1;
GLubyte* texData = 0;

PathTracer pathTracer;
bool shouldRedisplay = false;

void Reshape(GLFWwindow* window, int w, int h)
{
	glViewport(0, 0, w, h);
	//wWindow = w;
	//hWindow = h;
}

void DrawGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Draw GUI here

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Display()
{
	std::cout << "new frame" << std::endl;

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frameTex);
	int fbo_tex_loc = glGetUniformLocation(shaderProgram, "tex");
	glUniform1i(fbo_tex_loc, 0);
	glBindVertexArray(quadVao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // draw quad

	DrawGui();

	glfwSwapBuffers(window);
}

void Idle()
{
	if (shouldRedisplay)
	{
		glBindTexture(GL_TEXTURE_2D, frameTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wWindow, hWindow, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
		glBindTexture(GL_TEXTURE_2D, 0);
		Display();
		shouldRedisplay = false;
	}
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

void InitializeFrame()
{
	glGenVertexArrays(1, &quadVao);

	glGenTextures(1, &frameTex);
	glBindTexture(GL_TEXTURE_2D, frameTex);
	texData = new GLubyte[wWindow * hWindow * 3];
	for (int i = 0; i < wWindow * hWindow * 3; i++)
		texData[i] = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wWindow, hWindow, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	pathTracer.SetOutImage(texData);
}

void InitializePathTracer()
{
	pathTracer.SetResolution(glm::ivec2(wWindow, hWindow));
	
	pathTracer.SetCamera(glm::vec3(0.0f, 0.0f, -9.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	
	int id = pathTracer.LoadMesh("sphere.obj.object", glm::mat4(1.0f));
	Material m;
	m.base_color = glm::vec3(1.0f, 0.0f, 0.0f);
	m.metalness = 1.0f;
	m.roughness = 1.0f;
	pathTracer.SetMeshMaterial(id, m);

    id = pathTracer.LoadMesh("cube.obj.object", glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(4.0f)), glm::vec3(0.0f, -3.0f, 0.0f)), true);
	m.base_color = glm::vec3(1.0f, 1.0f, 0.0f);
	m.metalness = 1.0f;
	m.roughness = 0.5f;
	pathTracer.SetMeshMaterial(id, m);

	// Light
	id = pathTracer.LoadMesh("sphere.obj.object", glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 6.0f, -1.0f)), glm::vec3(0.5f)));
	m.emissive = glm::vec3(1.0);
	pathTracer.SetMeshMaterial(id, m);

	//pathTracer.AddLight(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(1.0f));
}

void PTRenderLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		pathTracer.RenderFrame();
		shouldRedisplay = true;
	}
}

void OnExit()
{
	if (texData)
	{
		delete texData;
		texData = 0;
	}
}

int main()
{
	InitializePathTracer();

	int initRes = InitializeGL(window);
	if (initRes)
		return initRes;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	InitializeFrame();

	omp_set_nested(1);
	#pragma omp parallel sections num_threads(2)
	{
		#pragma omp section
		while (!glfwWindowShouldClose(window))
		{
			Idle();
			//Display();
			glfwPollEvents();
		}
		#pragma omp section
		PTRenderLoop();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	OnExit();
	return 0;
}
