#define _USE_MATH_DEFINES

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>

#include <thread>

#include <math.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include <fonts/sourcesanspro.h> // Text font: Source Sans Pro
#include <fonts/forkawesome.h> // Icon font: Fork Awesome
#include <fonts/IconsForkAwesome.h> // Icon font header: Fork Awesome

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"

#include "tinyfiledialogs.h" // Cross-platform file dialogs library

#include "icon.h" // Application icon

#include "shaders.h"
#include "pathtracer.h"
#include "previewer.h"
#include "pathutil.h"

/* ----- GLFW/IMGUI PARAMS ------ */
GLFWwindow* window;
const GLint rightBarWidth = 380;
GLint menuHeight = 29;
const GLint toolbarHeight = 40;
const GLint statusBarHeight = 29;
GLint wRender = 1024;
GLint hRender = 768;
GLint wWindow = wRender + rightBarWidth;
GLint hWindow = hRender + menuHeight + toolbarHeight + statusBarHeight;

GLuint appIconTex = -1;

GLuint quadShader = -1;
GLuint previewShader = -1;
GLuint previewMaterialUBO = -1;

GLuint quadVao = -1;
GLuint fbo = -1;
GLuint rbo = -1;
GLuint frameTex = -1;
GLuint fboTex = -1;
GLuint pickTex = -1;
GLubyte* texData = 0;

ImFont* bigIconFont = 0;
ImFont* normalIconFont = 0;
/* ----- GLFW/IMGUI PARAMS ------ */

/* ----- PATHTRACER/PREVIEWER PARAMS ------ */
GLFWwindow* computeWindow;
GLuint computeProgram = -1;

bool threadRuning = true;

const std::string version = "2.0.0";

PathTracer pathTracer;
int traceDepth = 3;

bool autoRes = false;
bool resChanged = true;

int targetSample = 0;
bool showProgressBar = false;

bool render = false;
bool restart = false;
bool pause = false;
bool isPausing = false;
bool stop = false;
bool init = true;

bool canLoad = true;
bool canStart = true;
bool canPause = false;
bool canStop = false;
bool canRestart = false;

bool saveFile = false;
bool exportFile = false;
std::string exportFilePath = "";
std::string sceneFilePath = "";

bool sceneModified = false;
bool showSaveDialog = false;

bool saveAndNew = false;
bool saveAndOpen = false;
bool saveAndLoad = false;
bool saveAndExit = false;
std::string pendingLoadSceneFile = "";
bool execAfterSave = false;

std::string statusText = "";
std::chrono::steady_clock::time_point statusShowBegin;

float timePause = 0.0f;
float timeElapsed = 0.0f;

Previewer previewer;
bool preview = true;

bool viewportFocused = false;
bool cameraMouseControl = false;
double curPosDownX = 0.0;
double curPosDownY = 0.0;
glm::vec3 camRotDown;
float moveSpeed = 3.0f;
bool camMoveForward = false;
bool camMoveBackward = false;
bool camMoveLeft = false;
bool camMoveRight = false;

glm::vec3 previewBgColor = glm::vec3(0.0f);
glm::vec3 previewHighlightColor = glm::vec3(0.9f, 0.9f, 0.1f);
glm::vec3 previewSelectionColor = glm::vec3(0.1f, 0.7f, 0.9f);

std::string pwd_r = "";
std::string pwd_w = "";

int lastSelectedId = -1;
int pickedObjId = -1;
int pickedElementId = -1;

bool editingInput = false;
int undoCmd = -1;
int cutCmd = -1;
int copyCmd = -1;
int pasteCmd = -1;
int delCmd = -1;
int selectAllCmd = -1;

bool showSettingsWindow = false;
bool showAboutWindow = false;

struct RedirObject
{
	std::string name = "";
	std::string path = "";
	bool needsRedir = false;
};
std::vector<RedirObject> redirObjects;
std::vector<RedirObject> editingRedirObjects;
bool needsRedirObjects = false;
/* ----- PATHTRACER/PREVIEWER PARAMS ------ */

/* ----- TOOL FUNCTIONS ------ */
void ClearScene()
{
	previewer.ClearScene();

	preview = true;
	lastSelectedId = -1;

	traceDepth = 3;
	wRender = 1024;
	hRender = 768;
	autoRes = false;
	resChanged = true;

	previewer.SetCamera(glm::vec3(0.0f, 0.0f, -10.0f),
		previewer.CameraDirection(), previewer.CameraUp());
	previewer.RotateCamera(glm::vec3(0.0f));
}

void NewScene()
{
	if (sceneModified)
	{
		saveAndNew = true;
		showSaveDialog = true;
	}
	else
	{
		ClearScene();
		sceneFilePath = "";
		sceneModified = false;
	}
}

void GetResolutionFromSceneFile(const std::string& file)
{
	std::ifstream fr(file, std::ofstream::in);

	std::string line;
	if (!std::getline(fr, line)) { fr.close(); return; }
	if (line != "Path Tracer Scene File") { fr.close(); return; }
	if (!std::getline(fr, line)) { fr.close(); return; }
	std::string fileVersion = line.substr(line.find_first_of('=') + 1);
	if (fileVersion != version) { fr.close(); return; }
	if (fr.eof()) { fr.close(); return; }

	ClearScene();

	int ival = 0;
	float val = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	wRender = ival;
	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	hRender = ival;

	fr.close();
}

void LoadScene(const std::string& file)
{
	std::ifstream fr(file, std::ofstream::in);

	std::string line;
	if (!std::getline(fr, line)) { fr.close(); return; }
	if (line != "Path Tracer Scene File") { fr.close(); return; }
	if (!std::getline(fr, line)) { fr.close(); return; }
	std::string fileVersion = line.substr(line.find_first_of('=') + 1);
	if (fileVersion != version) { fr.close(); return; }
	if (fr.eof()) { fr.close(); return; }

	ClearScene();

	int ival = 0;
	float val = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	traceDepth = ival;

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	wRender = ival;
	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	hRender = ival;
	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	autoRes = (bool)ival;

	resChanged = true;

	fr >> x;
	if (fr.eof()) { fr.close(); return; }
	fr >> y;
	if (fr.eof()) { fr.close(); return; }
	fr >> z;
	if (fr.eof()) { fr.close(); return; }
	previewer.SetCamera(glm::vec3(x, y, z), previewer.CameraDirection(), previewer.CameraUp());
	fr >> x;
	if (fr.eof()) { fr.close(); return; }
	fr >> y;
	if (fr.eof()) { fr.close(); return; }
	fr >> z;
	if (fr.eof()) { fr.close(); return; }
	previewer.RotateCamera(glm::vec3(x, y, z));

	fr >> val;
	if (fr.eof()) { fr.close(); return; }
	previewer.SetCameraFocalDist(val);
	fr >> val;
	if (fr.eof()) { fr.close(); return; }
	previewer.SetCameraF(val);

	int nObjs = 0;
	fr >> nObjs;
	if (fr.eof()) { fr.close(); return; }
	if (!std::getline(fr, line)) { fr.close(); return; }
	for (int i = 0; i < nObjs; i++)
	{
		if (!std::getline(fr, line)) { fr.close(); return; }
		if (i <= redirObjects.size())
			previewer.LoadObject(redirObjects[i].path);
		else
			previewer.LoadObject(line);

		if (!std::getline(fr, line)) { fr.close(); return; }
		if (i <= redirObjects.size())
			previewer.SetName(i, redirObjects[i].name);
		else
			previewer.SetName(i, line);

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }
		previewer.SetLocation(i, glm::vec3(x, y, z));

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }
		previewer.SetRotation(i, glm::vec3(x, y, z));

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }
		previewer.SetScaleDirect(i, glm::vec3(x, y, z));

		int nElements = 0;
		fr >> nElements;
		if (fr.eof()) { fr.close(); return; }
		if (!std::getline(fr, line)) { fr.close(); return; }
		for (int j = 0; j < nElements; j++)
		{
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetName(i, j, line);

			Material m;

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }
			m.diffuse = glm::vec3(x, y, z);

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }
			m.specular = glm::vec3(x, y, z);

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }
			m.emissive = glm::vec3(x, y, z);

			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			m.emissiveIntensity = val;

			fr >> ival;
			if (fr.eof()) { fr.close(); return; }
			m.type = (MaterialType)ival;
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			m.roughness = val;
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			m.reflectiveness = val;
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			m.translucency = val;
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			m.ior = val;
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetDiffuseTextureForElement(i, j, line);
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetNormalTextureForElement(i, j, line);
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetEmissTextureForElement(i, j, line);
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetRoughnessTextureForElement(i, j, line);
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetMetallicTextureForElement(i, j, line);
			if (!std::getline(fr, line)) { fr.close(); return; }
			previewer.SetOpacityTextureForElement(i, j, line);

			previewer.SetMaterial(i, j, m);
		}
	}

	fr.close();

	editingRedirObjects.swap(std::vector<RedirObject>());
	redirObjects.swap(std::vector<RedirObject>());

	statusText = "Loaded scene: " + file;
	statusShowBegin = std::chrono::steady_clock::now();
}

// Pre-load objects for object paths redirecion
void LoadObjectPathsFromSceneFile(const std::string& file)
{
	redirObjects.swap(std::vector<RedirObject>());
	needsRedirObjects = false;

	std::ifstream fr(file, std::ofstream::in);

	std::string line;
	if (!std::getline(fr, line)) { fr.close(); return; }
	if (line != "Path Tracer Scene File") { fr.close(); return; }
	if (!std::getline(fr, line)) { fr.close(); return; }
	std::string fileVersion = line.substr(line.find_first_of('=') + 1);
	if (fileVersion != version) { fr.close(); return; }
	if (fr.eof()) { fr.close(); return; }

	ClearScene();
	sceneFilePath = PathUtil::UniversalPath(file);
	pwd_w = sceneFilePath.substr(0, sceneFilePath.find_last_of('/'));

	int ival = 0;
	float val = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	fr >> ival;
	if (fr.eof()) { fr.close(); return; }

	fr >> ival;
	if (fr.eof()) { fr.close(); return; }
	autoRes = (bool)ival;

	resChanged = true;

	fr >> x;
	if (fr.eof()) { fr.close(); return; }
	fr >> y;
	if (fr.eof()) { fr.close(); return; }
	fr >> z;
	if (fr.eof()) { fr.close(); return; }
	previewer.SetCamera(glm::vec3(x, y, z), previewer.CameraDirection(), previewer.CameraUp());
	fr >> x;
	if (fr.eof()) { fr.close(); return; }
	fr >> y;
	if (fr.eof()) { fr.close(); return; }
	fr >> z;
	if (fr.eof()) { fr.close(); return; }
	previewer.RotateCamera(glm::vec3(x, y, z));

	fr >> val;
	if (fr.eof()) { fr.close(); return; }
	fr >> val;
	if (fr.eof()) { fr.close(); return; }

	int nObjs = 0;
	fr >> nObjs;
	if (fr.eof()) { fr.close(); return; }
	if (!std::getline(fr, line)) { fr.close(); return; }
	for (int i = 0; i < nObjs; i++)
	{
		if (!std::getline(fr, line)) { fr.close(); return; }
		if (!previewer.LoadObject(line))
		{
			RedirObject obj;
			obj.path = line;
			if (!std::getline(fr, line)) { fr.close(); return; }
			obj.name = line;
			obj.needsRedir = true;
			redirObjects.push_back(obj);
			needsRedirObjects = true;
		}
		else
		{
			RedirObject obj;
			obj.path = line;
			if (!std::getline(fr, line)) { fr.close(); return; }
			obj.name = line;
			obj.needsRedir = false;
			redirObjects.push_back(obj);
		}

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }

		fr >> x;
		if (fr.eof()) { fr.close(); return; }
		fr >> y;
		if (fr.eof()) { fr.close(); return; }
		fr >> z;
		if (fr.eof()) { fr.close(); return; }

		int nElements = 0;
		fr >> nElements;
		if (fr.eof()) { fr.close(); return; }
		if (!std::getline(fr, line)) { fr.close(); return; }
		for (int j = 0; j < nElements; j++)
		{
			if (!std::getline(fr, line)) { fr.close(); return; }

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }

			fr >> x;
			if (fr.eof()) { fr.close(); return; }
			fr >> y;
			if (fr.eof()) { fr.close(); return; }
			fr >> z;
			if (fr.eof()) { fr.close(); return; }

			fr >> val;
			if (fr.eof()) { fr.close(); return; }

			fr >> ival;
			if (fr.eof()) { fr.close(); return; }
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			fr >> val;
			if (fr.eof()) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
			if (!std::getline(fr, line)) { fr.close(); return; }
		}
	}

	fr.close();

	editingRedirObjects = redirObjects;
	if (!needsRedirObjects)
		LoadScene(sceneFilePath);
	sceneModified = false;
}

void AfterSaving()
{
	if (saveAndNew)
	{
		ClearScene();
		sceneFilePath = "";
		sceneModified = false;

		saveAndNew = false;
	}
	else if (saveAndOpen)
	{
		const char* filterItems[1] = { "*.pts" };
		const char* filterDesc = "PathTracer Scene (*.pts)";
		auto ptsPath_c = tinyfd_openFileDialog("Open Scene",
			PathUtil::NativePath(sceneFilePath).c_str(),
			1, filterItems, filterDesc, 0);
		if (!ptsPath_c)
			return;
		LoadObjectPathsFromSceneFile(ptsPath_c);

		saveAndOpen = false;
	}
	else if (saveAndLoad)
	{
		LoadObjectPathsFromSceneFile(pendingLoadSceneFile);
		pendingLoadSceneFile = "";

		saveAndLoad = false;
	}
	else if (saveAndExit)
	{
		sceneModified = false;
		threadRuning = false;
		glfwSetWindowShouldClose(window, GLFW_TRUE);

		saveAndExit = false;
	}
}

// This function runs in path tracer thread
void SaveAt(std::string path)
{
	statusText = "Saving scene at: " + path + "...";
	statusShowBegin = std::chrono::steady_clock::now();

	std::ofstream fw(path, std::ofstream::out);

	fw << "Path Tracer Scene File\n" << "Version=" << version << "\n";

	fw << traceDepth << "\n";
	fw << wRender << " " << hRender << "\n";
	fw << (int)autoRes << "\n";

	glm::vec3 v3 = previewer.CameraPosition();
	fw << v3.x << " " << v3.y << " " << v3.z << "\n";
	v3 = previewer.CameraRotation();
	fw << v3.x << " " << v3.y << " " << v3.z << "\n";

	fw << previewer.CameraFocalDist() << "\n";
	fw << previewer.CameraF() << "\n";

	auto objs = previewer.GetLoadedObjects();
	fw << objs.size() << "\n";
	for (auto& obj : objs)
	{
		fw << obj.filename << "\n";
		fw << obj.name << "\n";
		v3 = obj.GetLocation();
		fw << v3.x << " " << v3.y << " " << v3.z << "\n";
		v3 = obj.GetRotation();
		fw << v3.x << " " << v3.y << " " << v3.z << "\n";
		v3 = obj.GetScale();
		fw << v3.x << " " << v3.y << " " << v3.z << "\n";
		fw << obj.elements.size() << "\n";
		for (auto& element : obj.elements)
		{
			fw << element.name << "\n";
			v3 = element.material.diffuse;
			fw << v3.x << " " << v3.y << " " << v3.z << "\n";
			v3 = element.material.specular;
			fw << v3.x << " " << v3.y << " " << v3.z << "\n";
			v3 = element.material.emissive;
			fw << v3.x << " " << v3.y << " " << v3.z << "\n";
			fw << element.material.emissiveIntensity << "\n";
			fw << (int)element.material.type << "\n";
			fw << element.material.roughness << "\n";
			fw << element.material.reflectiveness << "\n";
			fw << element.material.translucency << "\n";
			fw << element.material.ior << "\n";
			fw << element.diffuseTexFile << "\n";
			fw << element.normalTexFile << "\n";
			fw << element.emissTexFile << "\n";
			fw << element.roughnessTexFile << "\n";
			fw << element.metallicTexFile << "\n";
			fw << element.opacityTexFile << "\n";
		}
	}

	fw.close();

	statusText = "Saved scene at: " + path;
	statusShowBegin = std::chrono::steady_clock::now();
}

void Save()
{
	render = false;
	if (!stop && !init)
		pause = true;
	saveFile = true;
}

void SaveAs()
{
	std::string savePath = sceneFilePath;
	if (savePath.size() == 0)
	{
		savePath = pwd_w + "/Untitled";
	}

	const char* filterItems[1] = { "*.pts" };
	const char* filterDesc = "PathTracer Scene (*.pts)";
	auto ptsPath_c = tinyfd_saveFileDialog
	(
		"Save As", PathUtil::NativePath(savePath).c_str(),
		1, filterItems, filterDesc
	);
	if (ptsPath_c)
	{
		std::string ptsPath = PathUtil::UniversalPath(ptsPath_c);
		pwd_w = ptsPath.substr(0, ptsPath.find_last_of('/'));
		sceneFilePath = ptsPath;

		render = false;
		if (!stop && !init)
			pause = true;
		saveFile = true;
	}
	else
		AfterSaving();
}

void OpenScene()
{
	if (sceneModified)
	{
		saveAndOpen = true;
		showSaveDialog = true;
	}
	else
	{
		const char* filterItems[1] = { "*.pts" };
		const char* filterDesc = "PathTracer Scene (*.pts)";
		auto ptsPath_c = tinyfd_openFileDialog("Open Scene",
			PathUtil::NativePath(sceneFilePath).c_str(),
			1, filterItems, filterDesc, 0);
		if (!ptsPath_c)
			return;
		LoadObjectPathsFromSceneFile(ptsPath_c);
	}
}

// This function runs in path tracer thread
void ExportAt(const std::string& path)
{
    statusText = "Exporting file at: " + path + "...";
	statusShowBegin = std::chrono::steady_clock::now();

    stbi_flip_vertically_on_write(true);
    GLuint channel = 3; // rgb
    stbi_write_png(path.c_str(), wRender, hRender, channel, texData, channel * wRender);

	statusText = "Exported file at: " + path;
	statusShowBegin = std::chrono::steady_clock::now();
}

void Export()
{
	std::string scenename = sceneFilePath;
	if (scenename.size() == 0)
		scenename = "Untitled.pts";
	else
		scenename = scenename.substr(scenename.find_last_of('/') + 1);
	scenename = scenename.substr(0, scenename.find_last_of(".pts") - 3);

	exportFilePath = pwd_w + "/" + scenename + "_";
	auto now = std::chrono::system_clock::now();
	auto t = std::chrono::system_clock::to_time_t(now);
	exportFilePath += std::to_string(std::localtime(&t)->tm_year + 1900);
	exportFilePath += std::to_string(std::localtime(&t)->tm_mon);
	exportFilePath += std::to_string(std::localtime(&t)->tm_mday);
	exportFilePath += "_" + std::to_string(std::localtime(&t)->tm_hour);
	exportFilePath += "_" + std::to_string(std::localtime(&t)->tm_min);
	exportFilePath += "_" + std::to_string(std::localtime(&t)->tm_sec);
	exportFilePath += ".png";

	const char* filterItems[1] = { "*.png" };
	const char* filterDesc = "PNG (*.png)";
	auto pngPath_c = tinyfd_saveFileDialog
	(
		"Export As", PathUtil::NativePath(exportFilePath).c_str(),
		1, filterItems, filterDesc
	);
	if (pngPath_c)
	{
		std::string txtPath = PathUtil::UniversalPath(pngPath_c);
		pwd_w = txtPath.substr(0, txtPath.find_last_of('/'));
		exportFilePath = txtPath;

		render = false;
		if (!stop && !init)
			pause = true;
		exportFile = true;
	}
}

void LoadObject()
{
	const char* filterItems[1] = { "*.obj" };
	const char* filterDesc = "WaveFront (*.obj)";
	auto objPath_c = tinyfd_openFileDialog("Load Object", pwd_r.c_str(), 1, filterItems, filterDesc, 0);
	if (!objPath_c)
		return;

	std::string objPath = PathUtil::UniversalPath(objPath_c);
	pwd_r = objPath.substr(0, objPath.find_last_of('/'));
	previewer.LoadObject(objPath);
	preview = true;
	sceneModified = true;

	statusText = "Loaded object: " + objPath;
	statusShowBegin = std::chrono::steady_clock::now();
}

void ReplaceWith()
{
	const char* filterItems[1] = { "*.obj" };
	const char* filterDesc = "WaveFront (*.obj)";
	auto objPath_c = tinyfd_openFileDialog("Replace With", pwd_r.c_str(),
		1, filterItems, filterDesc, 0);
	if (objPath_c)
	{
		std::string objPath = PathUtil::UniversalPath(objPath_c);
		pwd_r = objPath.substr(0, objPath.find_last_of('/'));

		previewer.ReplaceSelectedObjectsWith(objPath);
		sceneModified = true;
	}
}

std::string LoadImage()
{
	const char* filterItems[5] = { "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tga"};
	const char* filterDesc = "Image Files (*.jpg;*.jpeg;*.png;*.bmp;*.tga)";
	auto imgPath_c = tinyfd_openFileDialog("Load Image", pwd_r.c_str(), 5, filterItems, filterDesc, 0);
	if (!imgPath_c)
		return "";
	return imgPath_c;
}
/* ----- TOOL FUNCTIONS ------ */

void InitializeFrame();
void InitializeGLFrame();

/* ----- GUI FUNCTIONS ------ */
void GuiInputContextMenu()
{
	if (!ImGui::IsItemActive())
		return;
	editingInput = true;

	auto activeId = ImGui::GetActiveID();
	auto curWin = ImGui::GetCurrentWindow();

	bool contextItemSelected = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
	ImGui::PushFont(normalIconFont);
	if (ImGui::BeginPopupContextItem())
	{
		auto mousePos = ImGui::GetMousePos();
		auto winPos = ImGui::GetWindowPos();
		auto winSize = ImGui::GetWindowSize();
		// The ImGui internal code has been hacked to handle the focus conflict
		// If the mouse click event happens inside the menu,
		// it will not dismiss the menu without losing focus on the text input widget
		if (mousePos.x > winPos.x && mousePos.x < winPos.x + winSize.x &&
			mousePos.y > winPos.y && mousePos.y < winPos.y + winSize.y)
			ImGui::GetIO().MouseInContextMenu = true;
		else
			ImGui::GetIO().MouseInContextMenu = false;

		int undoCount = ImGui::GetCurrentContext()->InputTextState.GetUndoAvailCount();
		bool hasSelection = ImGui::GetCurrentContext()->InputTextState.HasSelection();
		auto clipboardStr_c = ImGui::GetClipboardText();
		std::string clipboardStr = "";
		if (clipboardStr_c != NULL)
			clipboardStr = clipboardStr_c;

		if (ImGui::MenuItem("Undo", ICON_FK_UNDO, "CTRL+Z", false, undoCount != 0))
		{
			undoCmd = 0;
			contextItemSelected = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Cut", ICON_FK_SCISSORS, "CTRL+X", false, hasSelection))
		{
			cutCmd = 0;
			contextItemSelected = true;
		}
		if (ImGui::MenuItem("Copy", ICON_FK_FILES_O, "CTRL+C", false, hasSelection))
		{
			copyCmd = 0;
			contextItemSelected = true;
		}
		if (ImGui::MenuItem("Paste", ICON_FK_CLIPBOARD, "CTRL+V", false, clipboardStr.size() != 0))
		{
			pasteCmd = 0;
			contextItemSelected = true;
		}
		if (ImGui::MenuItem("Delete", ICON_FK_TRASH, "DELETE", false, hasSelection))
		{
			delCmd = 0;
			contextItemSelected = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Select All", "CTRL+A"))
		{
			selectAllCmd = 0;
			contextItemSelected = true;
		}

		ImGui::EndPopup();
	}
	else
		ImGui::GetIO().MouseInContextMenu = false;
	ImGui::PopFont();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	// Restore text input focus
	if (contextItemSelected)
		ImGui::SetActiveID(activeId, curWin);
}

// This function must run before drawing every ImGui frame
void ExecInputContextMenuCmd()
{
	editingInput = false;

	if (undoCmd >= 0)
	{
		if (undoCmd == 2)
		{
			ImGui::GetIO().AddKeyModsEvent(0);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, false);
			undoCmd = -1;
		}
		else if (undoCmd < 2)
		{
			ImGui::GetIO().AddKeyModsEvent(ImGuiKeyModFlags_Ctrl);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, true);
			undoCmd++;
		}
	}
	if (cutCmd >= 0)
	{
		if (cutCmd == 2)
		{
			ImGui::GetIO().AddKeyModsEvent(0);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_X, false);
			cutCmd = -1;
		}
		else if (cutCmd < 2)
		{
			ImGui::GetIO().AddKeyModsEvent(ImGuiKeyModFlags_Ctrl);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_X, true);
			cutCmd++;
		}
	}
	if (copyCmd >= 0)
	{
		if (copyCmd == 2)
		{
			ImGui::GetIO().AddKeyModsEvent(0);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_C, false);
			copyCmd = -1;
		}
		else if (copyCmd < 2)
		{
			ImGui::GetIO().AddKeyModsEvent(ImGuiKeyModFlags_Ctrl);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_C, true);
			copyCmd++;
		}
	}
	if (pasteCmd >= 0)
	{
		if (pasteCmd >= 2)
		{
			ImGui::GetIO().AddKeyModsEvent(0);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_V, false);
			pasteCmd = -1;
		}
		else if (pasteCmd < 2)
		{
			ImGui::GetIO().AddKeyModsEvent(ImGuiKeyModFlags_Ctrl);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_V, true);
			pasteCmd++;
		}
	}
	if (delCmd >= 0)
	{
		if (delCmd >= 2)
		{
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, false);
			delCmd = -1;
		}
		else if (delCmd < 2)
		{
			ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, true);
			delCmd++;
		}
	}
	if (selectAllCmd >= 0)
	{
		if (selectAllCmd >= 2)
		{
			ImGui::GetIO().AddKeyModsEvent(0);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
			selectAllCmd = -1;
		}
		else if (selectAllCmd < 2)
		{
			ImGui::GetIO().AddKeyModsEvent(ImGuiKeyModFlags_Ctrl);
			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
			selectAllCmd++;
		}
	}
}

void GuiMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));
	ImGui::PushFont(normalIconFont);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "CTRL+N", false, canLoad))
				NewScene();

			if (ImGui::MenuItem("Open Scene...", "CTRL+O", false, canLoad))
				OpenScene();

			ImGui::Separator();
			if (ImGui::MenuItem("Save", ICON_FK_FLOPPY_O, "CTRL+S", false, (!saveFile && !exportFile)))
			{
				if (sceneFilePath.size() == 0)
					SaveAs();
				else
					Save();
			}

			if (ImGui::MenuItem("Save As...", "CTRL+SHIFT+S", false, (!saveFile && !exportFile)))
				SaveAs();

			ImGui::Separator();
			if (ImGui::MenuItem("Load Object...", "CTRL+L", false, canLoad))
				LoadObject();

			if (ImGui::MenuItem("Export...", ICON_FK_SIGN_OUT, "CTRL+E", false, (!saveFile && !exportFile)))
				Export();

			ImGui::Separator();
			if (ImGui::MenuItem("Settings...", ICON_FK_COG, "", false))
				showSettingsWindow = true;

			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "ALT+F4"))
			{
				if (sceneModified)
				{
					saveAndExit = true;
					showSaveDialog = true;
				}
				else
				{
					threadRuning = false;
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			bool hasSelection = previewer.GetNumSelectedObjects() > 0;

			if (ImGui::MenuItem("Replace With...", "CTRL+R", false, hasSelection))
				ReplaceWith();
			if (ImGui::MenuItem("Delete", ICON_FK_TRASH, "DELETE", false, hasSelection))
			{
				lastSelectedId = -1;
				previewer.DeleteSelectedObjects();
				sceneModified = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Select All", "CTRL+A"))
			{
				for (int i = 0; i < previewer.GetLoadedObjects().size(); i++)
					previewer.SelectObject(i, true);
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Preview Mode", ICON_FK_DESKTOP, "SHIFT+1", false, !preview))
				preview = true;
			if (ImGui::MenuItem("Path Tracer Output", ICON_FK_PICTURE_O, "SHIFT+2", false, preview))
				preview = false;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render"))
		{
			if (ImGui::MenuItem("Start", ICON_FK_PLAY, "F5, SHIFT+B", false, canStart))
			{
				if (restart || init || stop)
				{
					init = true;
					InitializeFrame();
				}
				render = true;
				preview = false;
				glfwSetTime(timePause); // reset timer
			}
			if (ImGui::MenuItem("Pause", ICON_FK_PAUSE, "F6, SHIFT+P", false, canPause))
			{
				render = false;
				pause = true;
			}
			if (ImGui::MenuItem("Stop", ICON_FK_STOP, "F7, SHIFT+S", false, canStop))
			{
				render = false;
				restart = false;
				pause = false;
				isPausing = false;
				pathTracer.Exit();
				glfwSetTime(timePause); // reset timer
				stop = true;
			}
			if (ImGui::MenuItem("Resart", ICON_FK_UNDO, "F8, SHIFT+R", false, canRestart))
			{
				render = true;
				preview = false;
				restart = true;
				pathTracer.Exit();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About PathTracer...", ICON_FK_INFO_CIRCLE, ""))
				showAboutWindow = true;
			ImGui::EndMenu();
		}

		menuHeight = ImGui::GetWindowHeight();
		ImGui::EndMainMenuBar();
	}
	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void GuiToolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));

	ImGui::SetNextWindowPos(ImVec2(0, menuHeight));
	ImGui::SetNextWindowSize(ImVec2(wWindow, toolbarHeight));
	ImGui::Begin("Toolbar", 0,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBringToFrontOnFocus);

	ImGui::SetCursorPosY((ImGui::GetWindowHeight() - 30) * 0.5f);
	if (saveFile || exportFile)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::Button(ICON_FK_FLOPPY_O "   Save", ImVec2(0, 30));
		ImGui::PopStyleColor();
	}
	else
	{
		if (ImGui::Button(ICON_FK_FLOPPY_O "   Save", ImVec2(0, 30)))
		{
			if (sceneFilePath.size() == 0)
				SaveAs();
			else
				Save();
		}
	}

	ImGui::SameLine();
	if (saveFile || exportFile)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::Button(ICON_FK_SIGN_OUT "   Export", ImVec2(0, 30));
		ImGui::PopStyleColor();
	}
	else
	{
		if (ImGui::Button(ICON_FK_SIGN_OUT "   Export", ImVec2(0, 30)))
			Export();
	}

	ImGui::SameLine();
	ImGui::VerticalSeparator();

	ImGui::SameLine();
	ImGui::Text("  ");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160);
	ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) * 0.5f);
	int mode = (int)!preview;
	if (ImGui::Combo("##display_mode", &mode, "Preview Mode\0Path Tracer Output\0"))
		preview = !(bool)mode;
	ImGui::SameLine();
	ImGui::SetCursorPosY((ImGui::GetWindowHeight() - 30) * 0.5f);
	ImGui::Text("  ");
	ImGui::SameLine();
	ImGui::VerticalSeparator();

	ImGui::SameLine();
	if (!(init || stop) || render)
	{
		canLoad = false;
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::IconButton(ICON_FK_PLUS, "   Load Object", ImVec4(0.4f, 0.4f, 0.4f, 1.0f), ImVec2(0, 30));
		ImGui::PopStyleColor();
	}
	else
	{
		canLoad = true;
		if (ImGui::IconButton(ICON_FK_PLUS, "   Load Object", ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ImVec2(0, 30)))
			LoadObject();
	}
	ImGui::SameLine();
	ImGui::VerticalSeparator();
	ImGui::SameLine();
	ImGui::Text("  ");

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	ImGui::SameLine();
	ImGui::BeginChild("PathTracerControll", ImVec2(115, 30), true, ImGuiWindowFlags_NoScrollbar);

	ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) * 0.5f);
	if (!render)
	{
		if (pause || saveFile)
		{
			canStart = false;
			canPause = false;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			ImGui::SmallButton(ICON_FK_PLAY);
		}
		else
		{
			canStart = true;
			canPause = false;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
			if (ImGui::SmallButton(ICON_FK_PLAY))
			{
				if (restart || init || stop)
				{
					init = true;
					InitializeFrame();
				}
				render = true;
				preview = false;
				glfwSetTime(timePause); // reset timer
			}
		}
	}
	else
	{
		if (restart)
		{
			canStart = false;
			canPause = false;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			ImGui::SmallButton(ICON_FK_PLAY);
		}
		else
		{
			canStart = false;
			canPause = true;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			if (ImGui::SmallButton(ICON_FK_PAUSE))
			{
				render = false;
				pause = true;
			}
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	if (!render && !isPausing && !pause || restart || saveFile)
	{
		canStop = false;
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::SmallButton(ICON_FK_STOP);
	}
	else
	{
		canStop = true;
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		if (ImGui::SmallButton(ICON_FK_STOP))
		{
			render = false;
			restart = false;
			pause = false;
			isPausing = false;
			pathTracer.Exit();
			glfwSetTime(timePause); // reset timer
			stop = true;
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	if ((render || isPausing) && !saveFile)
	{
		if (restart || pause)
		{
			canRestart = false;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			ImGui::SmallButton(ICON_FK_UNDO);
		}
		else
		{
			canRestart = true;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			if (ImGui::SmallButton(ICON_FK_UNDO))
			{
				render = true;
				preview = false;
				restart = true;
				pathTracer.Exit();
			}
		}
	}
	else
	{
		canRestart = false;
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::SmallButton(ICON_FK_UNDO);
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::VerticalSeparator();
	ImGui::SameLine(ImGui::GetCursorPosX() + 90);
	if (ImGui::SmallButton(ICON_FK_ELLIPSIS_V))
	{
		ImVec2 popupPos = ImVec2
		(
			ImGui::GetItemRectMin().x - 3,
			ImGui::GetItemRectMax().y + 6
		);
		ImGui::SetNextWindowPos(popupPos);
		ImGui::OpenPopup("targetSampleConfig");
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
	if (ImGui::BeginPopup("targetSampleConfig"))
	{
		float curPosY = ImGui::GetCursorPosY();
		ImGui::SetCursorPosY(curPosY +
			(ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f);
		ImGui::Text("Target Sample");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::SetCursorPosY(curPosY);
		int val = targetSample;
		if (ImGui::InputInt("##targetSample", &val, 0))
		{
			if (val < 0)
				val = 0;
			if (val > 65535)
				val = 65535;
			targetSample = val;
		}
		GuiInputContextMenu();

		ImGui::EndPopup();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::SameLine(ImGui::GetContentRegionMax().x - 100.0f - ImGui::GetStyle().WindowPadding.x);
	if (ImGui::Button(ICON_FK_COG "   Settings", ImVec2(100, 30)))
		showSettingsWindow = true;

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void GuiRightBar()
{
	ImGui::SetNextWindowPos(ImVec2(wWindow - rightBarWidth, menuHeight + toolbarHeight));
	ImGui::SetNextWindowSize(ImVec2(rightBarWidth,
		hWindow - menuHeight - toolbarHeight - statusBarHeight));
	ImGui::Begin("Path Tracer", 0,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus);

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNodeEx("Image", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		if (!(init || stop) || render)
			ImGui::BeginDisabled();

		ImGui::Text("Trace Depth");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderInt("##traceDepth", &traceDepth, 1, 10, "%d",
			ImGuiSliderFlags_AlwaysClamp))
			sceneModified = true;
		GuiInputContextMenu();

		if (autoRes)
			ImGui::BeginDisabled();

		ImGui::Text("Resolution X");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(150);
		if (ImGui::DragInt("##resolutionX", &wRender, 1, 1, 5000, "%d",
			ImGuiSliderFlags_AlwaysClamp))
		{
			resChanged = true;
			sceneModified = true;
		}
		GuiInputContextMenu();

		ImGui::Text("Resolution Y");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(150);
		if (ImGui::DragInt("##resolutionY", &hRender, 1, 1, 5000, "%d",
			ImGuiSliderFlags_AlwaysClamp))
		{
			resChanged = true;
			sceneModified = true;
		}
		GuiInputContextMenu();

		if (autoRes)
			ImGui::EndDisabled();

		ImGui::Text("Auto Resolution");
		ImGui::SameLine(160);
		if (ImGui::Checkbox("##autoRes", &autoRes))
			sceneModified = true;

		if (!(init || stop) || render)
			ImGui::EndDisabled();
		ImGui::TreePop();
	}

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		if (!(init || stop) || render)
			ImGui::BeginDisabled();

		float fv = previewer.CameraFocalDist();
		ImGui::Text("Focal Distance");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderFloat("##focalDist", &fv, 0.f, 5.f, "%.2f",
			ImGuiSliderFlags_AlwaysClamp))
		{
			previewer.SetCameraFocalDist(fv);
			sceneModified = true;
		}
		GuiInputContextMenu();

		fv = previewer.CameraF();
		ImGui::Text("Camera F");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderFloat("##cameraF", &fv, 1.f, 32.f, "%.2f",
			ImGuiSliderFlags_AlwaysClamp))
		{
			previewer.SetCameraF(fv);
			sceneModified = true;
		}
		GuiInputContextMenu();

		float v3[3] = {};
		v3[0] = previewer.CameraPosition().x;
		v3[1] = previewer.CameraPosition().y;
		v3[2] = previewer.CameraPosition().z;
		ImGui::Text("Camera Location");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(200);
		if (ImGui::DragFloat3("##cameraPos", v3, 0.01f, 0.0f, 0.0f, "%.2f"))
		{
			previewer.SetCamera(glm::vec3(v3[0], v3[1], v3[2]),
				previewer.CameraDirection(), previewer.CameraUp());
			preview = true;
			sceneModified = true;
		}
		GuiInputContextMenu();

		v3[0] = previewer.CameraRotation().x;
		v3[1] = previewer.CameraRotation().y;
		v3[2] = previewer.CameraRotation().z;
		ImGui::Text("Camera Rotation");
		ImGui::SameLine(160);
		ImGui::SetNextItemWidth(200);
		if (ImGui::DragFloat3("##cameraRot", v3, 1.0f, 0.0f, 0.0f, "%.2f"))
		{
			previewer.RotateCamera(glm::vec3(v3[0], v3[1], v3[2]));
			preview = true;
			sceneModified = true;
		}
		GuiInputContextMenu();

		if (!(init || stop) || render)
			ImGui::EndDisabled();
		ImGui::TreePop();
	}

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		if (!(init || stop) || render)
			ImGui::BeginDisabled();

		int posY = ImGui::GetCursorPosY();
		ImGui::SetCursorPosY(posY + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f);
		int objCount = previewer.GetLoadedObjects().size();
		if (objCount < 2)
			ImGui::Text("%i object loaded", objCount);
		else
			ImGui::Text("%i objects loaded", objCount);

		ImGui::SameLine(ImGui::GetContentRegionMax().x - 80.0f - ImGui::GetStyle().WindowPadding.x);
		ImGui::SetCursorPosY(posY);
		if (ImGui::IconButton(ICON_FK_PLUS, "  Add##obj", ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ImVec2(70, 25)))
			LoadObject();

		auto& objs = previewer.GetLoadedObjects();
		int selectedId = -1;
		bool showContextMenu = false;
		for (int i = 0; i < objs.size(); i++)
		{
			bool highlightElement = false;
			std::string idStr = "##obj" + std::to_string(i);

			ImGuiTreeNodeFlags treeNodeFlags =
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			if (objs[i].isSelected)
				treeNodeFlags |= ImGuiTreeNodeFlags_Selected;

			if (pickedObjId != -1)
			{
				if (pickedObjId == i)
					ImGui::SetNextItemOpen(true);
				else
					ImGui::SetNextItemOpen(false);
			}
			bool nodeOpen = ImGui::TreeNodeEx(idStr.c_str(), treeNodeFlags, objs[i].name.c_str());
			if (!highlightElement)
				previewer.Highlight(i, ImGui::IsItemHovered());

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			ImGui::PushFont(normalIconFont);
			if (ImGui::BeginPopupContextItem())
			{
				showContextMenu = true;
				if (!objs[i].isSelected)
					selectedId = i;

				if (ImGui::MenuItem("Replace With...", "CTRL+R"))
					ReplaceWith();
				if (ImGui::MenuItem("Delete", ICON_FK_TRASH, "DELETE"))
				{
					lastSelectedId = -1;
					previewer.DeleteSelectedObjects();
					sceneModified = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Select All", "CTRL+A"))
				{
					for (int j = 0; j < objs.size(); j++)
						previewer.SelectObject(j, true);
					selectedId = -1;
				}

				ImGui::EndPopup();
			}
			ImGui::PopFont();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				selectedId = i;
			if (nodeOpen)
			{
				ImGui::Text("Name");
				ImGui::SameLine(160);
				ImGui::SetNextItemWidth(200);
				std::string objName(objs[i].name);
				idStr = "##objName" + std::to_string(i);
				if (ImGui::InputText(idStr.c_str(), &objName))
				{
					previewer.SetName(i, objName);
					sceneModified = true;
				}
				GuiInputContextMenu();

				float v3[3] = {};
				v3[0] = objs[i].GetLocation().x;
				v3[1] = objs[i].GetLocation().y;
				v3[2] = objs[i].GetLocation().z;
				ImGui::Text("Location");
				ImGui::SameLine(160);
				ImGui::SetNextItemWidth(200);
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				idStr = "##objPos" + std::to_string(i);
				if (ImGui::DragFloat3(idStr.c_str(), v3, 0.01f, 0.0f, 0.0f, "%.2f"))
				{
					previewer.SetLocation(i, glm::vec3(v3[0], v3[1], v3[2]));
					preview = true;
					sceneModified = true;
				}
				GuiInputContextMenu();

				v3[0] = objs[i].GetRotation().x;
				v3[1] = objs[i].GetRotation().y;
				v3[2] = objs[i].GetRotation().z;
				ImGui::Text("Rotation");
				ImGui::SameLine(160);
				ImGui::SetNextItemWidth(200);
				idStr = "##objRot" + std::to_string(i);
				if (ImGui::DragFloat3(idStr.c_str(), v3, 1.0f, 0.0f, 0.0f, "%.2f"))
				{
					previewer.SetRotation(i, glm::vec3(v3[0], v3[1], v3[2]));
					preview = true;
					sceneModified = true;
				}
				GuiInputContextMenu();

				v3[0] = objs[i].GetScale().x;
				v3[1] = objs[i].GetScale().y;
				v3[2] = objs[i].GetScale().z;
				ImGui::Text("Scale");
				ImGui::SameLine();
				ImGui::PushFont(normalIconFont);
				if (objs[i].isScaleLocked)
					ImGui::Text(" " ICON_FK_LOCK);
				else
					ImGui::Text(" " ICON_FK_UNLOCK);
				ImGui::PopFont();
				if (ImGui::IsItemClicked())
					previewer.SetScaleLocked(i, !objs[i].isScaleLocked);
				ImGui::SameLine(160);
				ImGui::SetNextItemWidth(200);
				idStr = "##objScale" + std::to_string(i);
				if (ImGui::DragFloat3(idStr.c_str(), v3, 0.01f, 0.0f, 0.0f, "%.2f"))
				{
					previewer.SetScale(i, glm::vec3(v3[0], v3[1], v3[2]));
					preview = true;
					sceneModified = true;
				}
				GuiInputContextMenu();

				for (int j = 0; j < objs[i].elements.size(); j++)
				{
					std::string elementName = objs[i].elements[j].name;
					if (elementName.size() == 0)
						elementName = "Element " + std::to_string(j);
					idStr = "##element";
					idStr += std::to_string(i) + "_" + std::to_string(j);

					if (pickedObjId == i && pickedElementId != -1)
					{
						if (pickedElementId == j)
							ImGui::SetNextItemOpen(true);
						else
							ImGui::SetNextItemOpen(false);
					}
					if (ImGui::TreeNodeEx(idStr.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth,
						elementName.c_str()))
					{
						ImGui::Text("Name");
						ImGui::SameLine(160);
						ImGui::SetNextItemWidth(200);
						std::string elementName(objs[i].elements[j].name);
						std::string idSubStr = "##name" + idStr;
						if (ImGui::InputText(idSubStr.c_str(), &elementName))
						{
							previewer.SetName(i, j, elementName);
							sceneModified = true;
						}
						GuiInputContextMenu();

						ImGui::Text("Diffuse Color");
						ImGui::SameLine(160);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
						ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
						ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
						v3[0] = objs[i].elements[j].material.diffuse.r;
						v3[1] = objs[i].elements[j].material.diffuse.g;
						v3[2] = objs[i].elements[j].material.diffuse.b;
						idSubStr = "##diffuseCol" + idStr;
						if (ImGui::ColorEdit3(idSubStr.c_str(), v3,
							ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
						{
							Material& m = objs[i].elements[j].material;
							m.diffuse = glm::vec3(v3[0], v3[1], v3[2]);
							previewer.SetMaterial(i, j, m);
							preview = true;
							sceneModified = true;
						}
						ImGui::PopStyleColor();
						ImGui::PopStyleVar();
						ImGui::PopStyleVar();

						ImGui::Text("Specular Color");
						ImGui::SameLine(160);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
						ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
						ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
						v3[0] = objs[i].elements[j].material.specular.r;
						v3[1] = objs[i].elements[j].material.specular.g;
						v3[2] = objs[i].elements[j].material.specular.b;
						idSubStr = "##SpecularCol" + idStr;
						if (ImGui::ColorEdit3(idSubStr.c_str(), v3,
							ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
						{
							Material& m = objs[i].elements[j].material;
							m.specular = glm::vec3(v3[0], v3[1], v3[2]);
							previewer.SetMaterial(i, j, m);
							preview = true;
							sceneModified = true;
						}
						ImGui::PopStyleColor();
						ImGui::PopStyleVar();
						ImGui::PopStyleVar();

						ImGui::Text("Emissive Color");
						ImGui::SameLine(160);
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
						ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
						ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
						v3[0] = objs[i].elements[j].material.emissive.r;
						v3[1] = objs[i].elements[j].material.emissive.g;
						v3[2] = objs[i].elements[j].material.emissive.b;
						idSubStr = "##emisCol" + idStr;
						if (ImGui::ColorEdit3(idSubStr.c_str(), v3,
							ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
						{
							Material& m = objs[i].elements[j].material;
							m.emissive = glm::vec3(v3[0], v3[1], v3[2]);
							previewer.SetMaterial(i, j, m);
							preview = true;
							sceneModified = true;
						}
						ImGui::PopStyleColor();
						ImGui::PopStyleVar();
						ImGui::PopStyleVar();

						ImGui::Text("Emissive Intensity");
						ImGui::SameLine(160);
						ImGui::SetNextItemWidth(150);
						float val = objs[i].elements[j].material.emissiveIntensity;
						idSubStr = "##emisIntensity" + idStr;
						if (ImGui::DragFloat(idStr.c_str(), &val, 0.1f, 0.0f, 65535.0f, "%.2f",
							ImGuiSliderFlags_AlwaysClamp))
						{
							Material& m = objs[i].elements[j].material;
							m.emissiveIntensity = val;
							previewer.SetMaterial(i, j, m);
							preview = true;
							sceneModified = true;
						}
						GuiInputContextMenu();

						ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
							(ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Material Type");
						ImGui::SameLine(160);
						ImGui::SetNextItemWidth(150);
						idSubStr = "##mat" + idStr;
						int iVal = (int)objs[i].elements[j].material.type;
						const char* items = "Opaque\0Translucent";
						if (ImGui::Combo(idSubStr.c_str(), &iVal, items))
						{
							Material& m = objs[i].elements[j].material;
							m.type = (MaterialType)iVal;
							previewer.SetMaterial(i, j, m);
							sceneModified = true;
						}

						val = objs[i].elements[j].material.roughness;
						ImGui::Text("Roughness");
						ImGui::SameLine(160);
						ImGui::SetNextItemWidth(150);
						idSubStr = "##roughness" + idStr;
						if (ImGui::SliderFloat(idSubStr.c_str(), &val, 0.0f, 1.0f, "%.2f",
							ImGuiSliderFlags_AlwaysClamp))
						{
							Material& m = objs[i].elements[j].material;
							m.roughness = val;
							previewer.SetMaterial(i, j, m);
							sceneModified = true;
						}

						val = objs[i].elements[j].material.reflectiveness;
						ImGui::Text("Reflectiveness");
						ImGui::SameLine(160);
						ImGui::SetNextItemWidth(150);
						idSubStr = "##reflectiveness" + idStr;
						if (ImGui::SliderFloat(idSubStr.c_str(), &val, 0.0f, 1.0f, "%.2f",
							ImGuiSliderFlags_AlwaysClamp))
						{
							Material& m = objs[i].elements[j].material;
							m.reflectiveness = val;
							previewer.SetMaterial(i, j, m);
							sceneModified = true;
						}

						if (objs[i].elements[j].material.type == MaterialType::TRANSLUCENT)
						{
							val = objs[i].elements[j].material.translucency;
							ImGui::Text("Translucency");
							ImGui::SameLine(160);
							ImGui::SetNextItemWidth(150);
							idSubStr = "##translucency" + idStr;
							if (ImGui::SliderFloat(idSubStr.c_str(), &val, 0.0f, 1.0f, "%.2f",
								ImGuiSliderFlags_AlwaysClamp))
							{
								Material& m = objs[i].elements[j].material;
								m.translucency = val;
								previewer.SetMaterial(i, j, m);
								sceneModified = true;
							}

							val = objs[i].elements[j].material.ior;
							ImGui::Text("IOR");
							ImGui::SameLine(160);
							ImGui::SetNextItemWidth(150);
							idSubStr = "##ior" + idStr;
							if (ImGui::InputFloat(idSubStr.c_str(), &val, 0.0f, 0.0f, "%.7f"))
							{
								if (val < 1.0f)
									val = 1.0f;
								else if (val > 5.0f)
									val = 5.0f;
								Material& m = objs[i].elements[j].material;
								m.ior = val;
								previewer.SetMaterial(i, j, m);
								sceneModified = true;
							}
							GuiInputContextMenu();
						}

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Diffuse Texture");

						GLuint texId = objs[i].elements[j].diffuseTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						int posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##diffuseTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetDiffuseTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##diffuseTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetDiffuseTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Normal Texture");

						texId = objs[i].elements[j].normalTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##normalmap" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetNormalTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##normalmap" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetNormalTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Emissive Texture");

						texId = objs[i].elements[j].emissTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##emissTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetEmissTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##emissTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetEmissTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Roughness Texture");

						texId = objs[i].elements[j].roughnessTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##roughnessTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetRoughnessTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##roughnessTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetRoughnessTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Metallic Texture");

						texId = objs[i].elements[j].metallicTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##metallicTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetMetallicTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##metallicTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetMetallicTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						posY = ImGui::GetCursorPosY();
						ImGui::SetCursorPosY(posY + (50 - ImGui::GetTextLineHeight()) * 0.5f);
						ImGui::Text("Opacity Texture");

						texId = objs[i].elements[j].opacityTexId;
						ImGui::SameLine(160);
						ImGui::SetCursorPosY(posY);
						ImGui::Image((void*)texId, ImVec2(50, 50));

						ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
						ImGui::PushFont(normalIconFont);
						ImGui::SameLine();
						posX = ImGui::GetCursorPosX();
						ImGui::SetCursorPosY(posY);
						idSubStr = "Load##opacityTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							std::string imgPath = LoadImage();
							if (imgPath.size() != 0)
							{
								previewer.SetOpacityTextureForElement(i, j, imgPath);
								preview = true;
								sceneModified = true;
							}
						}

						ImGui::SetCursorPosX(posX);
						ImGui::SetCursorPosY(posY + 27);
						if (texId == -1)
							ImGui::BeginDisabled();
						idSubStr = "Remove##opacityTex" + idStr;
						if (ImGui::Button(idSubStr.c_str(), ImVec2(65, 23)))
						{
							previewer.SetOpacityTextureForElement(i, j, "");
							preview = true;
							sceneModified = true;
						}
						if (texId == -1)
							ImGui::EndDisabled();
						ImGui::PopFont();
						ImGui::PopStyleVar();

						ImGui::TreePop();
					}
					if (ImGui::IsItemHovered())
						highlightElement = true;
					previewer.Highlight(i, j, ImGui::IsItemHovered());
				}

				ImGui::TreePop();
			}
		}
		pickedObjId = -1;
		pickedElementId = -1;
		if (selectedId != -1)
		{
			bool multiSelections = previewer.GetNumSelectedObjects() > 1;

			if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
			{
				for (int i = 0; i < objs.size(); i++)
					previewer.SelectObject(i, false);
			}
			if (objs[selectedId].isSelected && (!multiSelections || ImGui::GetIO().KeyCtrl))
				previewer.SelectObject(selectedId, false);
			else
				previewer.SelectObject(selectedId, true);

			if (ImGui::GetIO().KeyShift && lastSelectedId != -1)
			{
				if (lastSelectedId < selectedId)
				{
					for (int i = lastSelectedId + 1; i <= selectedId; i++)
						previewer.SelectObject(i, !objs[i].isSelected);
				}
				else
				{
					for (int i = selectedId; i < lastSelectedId; i++)
						previewer.SelectObject(i, !objs[i].isSelected);
				}
			}
			lastSelectedId = selectedId;
		}

		if (!(init || stop) || render)
			ImGui::EndDisabled();
		ImGui::TreePop();
	}

	ImGui::End();
}

void GuiMainViewport()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowPos(ImVec2(0, menuHeight + toolbarHeight));
	ImGui::SetNextWindowSize(ImVec2(wWindow - rightBarWidth,
		hWindow - menuHeight - toolbarHeight - statusBarHeight));
	ImGui::Begin("Viewport", 0,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::BeginChild("ViewportRender");
	ImGui::Image((ImTextureID)fboTex, ImGui::GetWindowSize(), ImVec2(0, 1), ImVec2(1, 0));
	viewportFocused = ImGui::IsItemHovered();
	ImGui::EndChild();
	ImGui::End();
	ImGui::PopStyleVar();
}

void GuiStausBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
	ImGui::PushFont(normalIconFont);

	ImGui::SetNextWindowPos(ImVec2(0, hWindow - statusBarHeight));
	ImGui::SetNextWindowSize(ImVec2(wWindow, statusBarHeight));
	ImGui::Begin("statusbar", 0,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBringToFrontOnFocus);

	if (statusText.size() != 0)
	{
		auto now = std::chrono::steady_clock::now();
		auto diff = now - statusShowBegin;
		if (diff > std::chrono::seconds(5))
			statusText = "";
	}
	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 850 - 40);
	ImGui::TextClipped(statusText.c_str());

	ImGui::SameLine(ImGui::GetWindowWidth() - 840 - 40);
	ImGui::VerticalSeparator();
	ImGui::SameLine();
	if (showProgressBar && pathTracer.GetSamples() < targetSample)
	{
		float curPosX = ImGui::GetCursorPosX();
		ImGui::Text("         ");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		float progress = (float)pathTracer.GetSamples() / (float)targetSample;
		ImGui::ProgressBar(progress, ImVec2(180, 23));
		ImGui::PopStyleColor();
		ImGui::SameLine(curPosX);
	}
	ImGui::SetNextItemWidth(210);
	if (!render)
	{
		if (!pause && !isPausing && !restart || stop)
		{
			showProgressBar = false;
			ImGui::TextClipped(ICON_FK_CIRCLE_O_NOTCH "  Idle");
		}
		else
		{
			showProgressBar = true;
			ImGui::TextClipped(ICON_FK_CIRCLE_O_NOTCH "  Pausing,  samples: %.i", pathTracer.GetSamples());
		}
	}
	else
	{
		showProgressBar = true;
		ImGui::TextClipped(ICON_FK_PENCIL_SQUARE_O "  Rendering,  samples: %.i", pathTracer.GetSamples());
	}

	ImGui::SameLine(ImGui::GetWindowWidth() - 620 - 40);
	ImGui::VerticalSeparator();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220);
	if (!render)
	{
		if (pathTracer.GetSamples() == 0 || stop)
			ImGui::TextClipped(ICON_FK_TACHOMETER "  Avg Time per Sample: %.2f s", 0.0f);
		else
			ImGui::TextClipped(ICON_FK_TACHOMETER "  Avg Time per Sample: %.2f s", timePause / pathTracer.GetSamples());
	}
	else
	{
		if (pathTracer.GetSamples() == 0)
			ImGui::TextClipped(ICON_FK_TACHOMETER "  Avg Time per Sample: %.2f s", 0.0f);
		else
			ImGui::TextClipped(ICON_FK_TACHOMETER "  Avg Time per Sample: %.2f s", glfwGetTime() / pathTracer.GetSamples());
	}

	ImGui::SameLine(ImGui::GetWindowWidth() - 390 - 40);
	ImGui::VerticalSeparator();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200);
	if (stop)
		ImGui::TextClipped(ICON_FK_CLOCK_O "  Time Elapsed : % .2f s", 0.0f);
	else
		ImGui::TextClipped(ICON_FK_CLOCK_O "  Time Elapsed : % .2f s", (render || pause) ? glfwGetTime() : timePause);

	ImGui::SameLine(ImGui::GetWindowWidth() - 180 - 40);
	ImGui::VerticalSeparator();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(210);
	ImGui::TextClipped(ICON_FK_CUBES "  Triangle Count: %.i", previewer.GetTriangleCount());

	ImGui::End();
	ImGui::PopFont();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void GuiSettingsWindow()
{
	ImGui::SetNextWindowPos
	(
		ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
		ImGuiCond_Once, ImVec2(0.5f, 0.5f)
	);
	ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints(ImVec2(500, 100), ImVec2(INF, INF));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.4f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushFont(normalIconFont);

	ImGui::Begin("Settings", &showSettingsWindow,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNodeEx("Previewer##settings", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		ImGui::Text("     Camera Navigation Speed");
		ImGui::SameLine(250);
		int iMoveSpeed = moveSpeed;
		ImGui::SetNextItemWidth(150);
		if (ImGui::SliderInt("##moveSpeed", &iMoveSpeed, 1, 10, "%d",
			ImGuiSliderFlags_AlwaysClamp))
			moveSpeed = iMoveSpeed;
		GuiInputContextMenu();

		float xPos = ImGui::GetCursorPosX();
		ImGui::SetCursorPosX(250);
		float v3[3]{};
		v3[0] = previewBgColor.r;
		v3[1] = previewBgColor.g;
		v3[2] = previewBgColor.b;
		if (ImGui::ColorEdit3("##bgCol", v3,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
			previewBgColor = glm::vec3(v3[0], v3[1], v3[2]);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::SameLine();
		ImGui::SetCursorPosX(xPos);
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNodeEx("Background Color", ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			float val = previewBgColor.r;
			ImGui::Text("        R");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##bgColR", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewBgColor = glm::vec3(val, v3[1], v3[2]);
			GuiInputContextMenu();

			val = previewBgColor.g;
			ImGui::Text("        G");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##bgColG", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewBgColor = glm::vec3(v3[0], val, v3[2]);
			GuiInputContextMenu();

			val = previewBgColor.b;
			ImGui::Text("        B");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##bgColB", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewBgColor = glm::vec3(v3[0], v3[1], val);
			GuiInputContextMenu();

			ImGui::TreePop();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::SetCursorPosX(250);
		v3[0] = previewHighlightColor.r;
		v3[1] = previewHighlightColor.g;
		v3[2] = previewHighlightColor.b;
		if (ImGui::ColorEdit3("##hlCol", v3,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
			previewHighlightColor = glm::vec3(v3[0], v3[1], v3[2]);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::SameLine();
		ImGui::SetCursorPosX(xPos);
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNodeEx("Highlight Color", ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			float val = previewHighlightColor.r;
			ImGui::Text("        R");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##hlColR", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewHighlightColor = glm::vec3(val, v3[1], v3[2]);
			GuiInputContextMenu();

			val = previewHighlightColor.g;
			ImGui::Text("        G");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##hlColG", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewHighlightColor = glm::vec3(v3[0], val, v3[2]);
			GuiInputContextMenu();

			val = previewHighlightColor.b;
			ImGui::Text("        B");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##hlColB", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewHighlightColor = glm::vec3(v3[0], v3[1], val);
			GuiInputContextMenu();

			ImGui::TreePop();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8, 8));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::SetCursorPosX(250);
		v3[0] = previewSelectionColor.r;
		v3[1] = previewSelectionColor.g;
		v3[2] = previewSelectionColor.b;
		if (ImGui::ColorEdit3("##slCol", v3,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip))
			previewSelectionColor = glm::vec3(v3[0], v3[1], v3[2]);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::SameLine();
		ImGui::SetCursorPosX(xPos);
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNodeEx("Selection Color", ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			float val = previewSelectionColor.r;
			ImGui::Text("        R");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##slColR", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewSelectionColor = glm::vec3(val, v3[1], v3[2]);
			GuiInputContextMenu();

			val = previewSelectionColor.g;
			ImGui::Text("        G");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##slColG", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewSelectionColor = glm::vec3(v3[0], val, v3[2]);
			GuiInputContextMenu();

			val = previewSelectionColor.b;
			ImGui::Text("        B");
			ImGui::SameLine(250);
			ImGui::SetNextItemWidth(150);
			if (ImGui::DragFloat("##slColB", &val, 0.01f, 0.0f, 1.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp))
				previewSelectionColor = glm::vec3(v3[0], v3[1], val);
			GuiInputContextMenu();

			ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	ImGui::PopStyleVar();
	ImGui::End();

	ImGui::PopFont();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void GuiRedirObjectsWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 20));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	ImGui::OpenPopup("Objects Redirection");
	ImGui::SetNextWindowPos
	(
		ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)
	);
	ImGui::SetNextWindowSize(ImVec2(556, 600), ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(INF, INF));
	if (ImGui::BeginPopupModal("Objects Redirection", NULL))
	{
		float winWidth = ImGui::GetWindowWidth();
		float winHeight = ImGui::GetWindowHeight();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 16));

		std::string str = "The following objects are missing their obj files.\n";
		str += "Please specify new paths for them:\n";
		ImGui::Text(str.c_str());

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		float childHeight = ImGui::GetContentRegionAvail().y - 61;
		ImGui::BeginChild("redirObjs", ImVec2(winWidth - 56, childHeight), true);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));
		for (int i = 0; i < editingRedirObjects.size(); i++)
		{
			if (!editingRedirObjects[i].needsRedir)
				continue;
			ImGui::SetNextItemWidth(124);
			ImGui::TextClipped(editingRedirObjects[i].name.c_str());

			std::string idStr = "##redirObjPath" + std::to_string(i);
			ImGui::SameLine(160);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 112);
			ImGui::InputText(idStr.c_str(), &editingRedirObjects[i].path);
			GuiInputContextMenu();

			ImGui::SameLine();
			ImGui::SameLine(ImGui::GetCursorPosX() + 6);
			if (ImGui::Button("Browse...", ImVec2(80, 0)))
			{
				const char* filterItems[1] = { "*.obj" };
				const char* filterDesc = "WaveFront (*.obj)";
				auto objPath_c = tinyfd_openFileDialog("Browse Object", pwd_r.c_str(),
					1, filterItems, filterDesc, 0);
				if (objPath_c)
				{
					std::string objPath = PathUtil::UniversalPath(objPath_c);
					pwd_r = objPath.substr(0, objPath.find_last_of('/'));
					editingRedirObjects[i].path = objPath;
				}
			}
		}
		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::SetCursorPosX(winWidth - 28 - 326);
		if (ImGui::Button("OK", ImVec2(160, 25)))
		{
			for (int i = 0; i < editingRedirObjects.size(); i++)
			{
				if (redirObjects[i].path != editingRedirObjects[i].path)
				{
					redirObjects[i].path = editingRedirObjects[i].path;
					sceneModified = true;
				}
			}

			ImGui::CloseCurrentPopup();
			needsRedirObjects = false;
			LoadScene(sceneFilePath);
		}
		ImGui::SetItemDefaultFocus();

		ImGui::SameLine(winWidth - 28 - 160);
		if (ImGui::Button("Cancel", ImVec2(160, 25)))
		{
			ImGui::CloseCurrentPopup();
			needsRedirObjects = false;
			LoadScene(sceneFilePath);
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void GuiSaveDialog()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 20));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	ImGui::OpenPopup("PathTracer##SaveDialog");
	
	ImGui::SetNextWindowPos
	(
		ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)
	);
	if (ImGui::BeginPopupModal("PathTracer##SaveDialog", NULL,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 16));

		std::string filename = sceneFilePath;
		if (filename.size() == 0)
			filename = "Untitled.pts";
		else
			filename = filename.substr(filename.find_last_of('/') + 1);

		std::string str = "Save changes before closing?\n";
		str += filename;
		ImGui::Text(str.c_str());

		if (ImGui::Button("Yes", ImVec2(120, 25)))
		{
			saveAndExit = true;
			ImGui::CloseCurrentPopup();
			showSaveDialog = false;
			if (sceneFilePath.size() == 0)
				SaveAs();
			else
				Save();
		}
		ImGui::SetItemDefaultFocus();

		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 25)))
		{
			saveAndExit = true;
			ImGui::CloseCurrentPopup();
			showSaveDialog = false;

			AfterSaving();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 25)))
		{
			ImGui::CloseCurrentPopup();
			showSaveDialog = false;

			saveAndNew = false;
			saveAndOpen = false;
			saveAndLoad = false;
			saveAndExit = false;
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void GuiAboutWindow()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 20));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

	ImGui::SetNextWindowPos
	(
		ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)
	);
	ImGui::SetNextWindowSize(ImVec2(450, 250));
	ImGui::Begin("About PathTracer", &showAboutWindow,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoResize);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
	ImGui::BeginChild("aboutIcon", ImVec2(450 - 56, 140));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));

	ImGui::Image((void*)appIconTex, ImVec2(120, 120));

	ImGui::SameLine(140);
	ImGui::BeginChild("aboutText", ImVec2(250, 120));

	ImGui::Text("PathTracer");
	std::string versionStr = "Version: " + version;
	ImGui::Text(versionStr.c_str());

	ImGui::EndChild();

	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::SetCursorPosX(450 - 28 - 120);
	if (ImGui::Button("OK", ImVec2(120, 25)))
	{
		ImGui::CloseCurrentPopup();
		showAboutWindow = false;
	}
	ImGui::SetItemDefaultFocus();

	ImGui::PopStyleVar();
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void DrawGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ExecInputContextMenuCmd();
	
	GuiMenuBar();

	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));

	GuiToolbar();

	GuiRightBar();
	
	GuiMainViewport();
	GuiStausBar();

	if (showSettingsWindow)
		GuiSettingsWindow();
	if (needsRedirObjects)
		GuiRedirObjectsWindow();
	if (showSaveDialog)
		GuiSaveDialog();
	if (showAboutWindow)
		GuiAboutWindow();

	ImGui::PopStyleColor();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
/* ----- GUI FUNCTIONS ------ */

/* ----- GLFW FUNCTIONS ------ */
void Display()
{
	int wViewport = wWindow - rightBarWidth;
	int hViewport = hWindow - menuHeight - toolbarHeight - statusBarHeight;
	if (autoRes && canLoad)
	{
		if (wRender != wViewport || hRender != hViewport)
		{
			wRender = wViewport;
			hRender = hViewport;
			if (wRender < 0)
				wRender = 0;
			if (hRender < 0)
				hRender = 0;
			resChanged = true;
		}
	}
	if (resChanged)
	{
		InitializeGLFrame();
		resChanged = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, wRender, hRender);

	if (preview)
	{
		glUseProgram(previewShader);

		glm::vec3 eyePos = previewer.PreviewCameraPosition();
		glm::mat4 V = glm::lookAt(eyePos, eyePos + previewer.PreviewCameraDirection(), previewer.PreviewCameraUp());
		glm::mat4 P = glm::perspective(previewer.CameraFovy(), (float)wRender / (float)hRender, previewer.CameraFocal(), 100.0f);
		glm::mat4 PV = P * V;
		glUniformMatrix4fv(0, 1, false, glm::value_ptr(PV));
		glUniform3fv(2, 1, glm::value_ptr(eyePos));

		for (int pass = 0; pass < 2; pass++)
		{
			glUniform1i(7, pass);
			glDrawBuffer(GL_COLOR_ATTACHMENT0 + pass);
			if (pass == 0)
				glClearColor(previewBgColor.r, previewBgColor.g, previewBgColor.b, 1.0f);
			else
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			auto& objs = previewer.GetLoadedObjects();
			for (int i = 0; i < objs.size(); i++)
			{
				glUniform1i(8, i + 1);

				glUniformMatrix4fv(1, 1, false, glm::value_ptr(objs[i].Mpreview));

				for (int j = 0; j < objs[i].elements.size(); j++)
				{
					glUniform1i(9, j + 1);

					glm::vec3 color = objs[i].elements[j].material.diffuse;
					if (objs[i].elements[j].highlight)
						color = previewHighlightColor;
					else if (objs[i].isSelected)
						color = previewSelectionColor;

					glUniform1i(10, objs[i].elements[j].highlight || objs[i].isSelected);

					glBindBufferBase(GL_UNIFORM_BUFFER, 0, previewMaterialUBO);
					GLubyte* blockData = (GLubyte*)glMapBufferRange
					(
						GL_UNIFORM_BUFFER, 0, 56,
						GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT
					);
					glm::vec4 v4 = glm::vec4(color, 1.0f);
					memcpy(blockData + 0, glm::value_ptr(v4), sizeof(float) * 4);
					v4 = glm::vec4(objs[i].elements[j].material.specular, 1.0f);
					memcpy(blockData + 16, glm::value_ptr(v4), sizeof(float) * 4);
					float v = objs[i].elements[j].material.roughness;
					memcpy(blockData + 32, &v, sizeof(float));
					v = objs[i].elements[j].material.reflectiveness;
					memcpy(blockData + 36, &v, sizeof(float));
					int iv = objs[i].elements[j].diffuseTexId != -1;
					memcpy(blockData + 40, &iv, sizeof(int));
					iv = objs[i].elements[j].normalTexId != -1;
					memcpy(blockData + 44, &iv, sizeof(int));
					iv = objs[i].elements[j].roughnessTexId != -1;
					memcpy(blockData + 48, &iv, sizeof(int));
					iv = objs[i].elements[j].metallicTexId != -1;
					memcpy(blockData + 52, &iv, sizeof(int));
					glUnmapBuffer(GL_UNIFORM_BUFFER);

					if (objs[i].elements[j].diffuseTexId != -1)
					{
						glActiveTexture(GL_TEXTURE0 + 0);
						glBindTexture(GL_TEXTURE_2D, objs[i].elements[j].diffuseTexId);
						glUniform1i(3, 0);
					}
					if (objs[i].elements[j].normalTexId != -1)
					{
						glActiveTexture(GL_TEXTURE0 + 1);
						glBindTexture(GL_TEXTURE_2D, objs[i].elements[j].normalTexId);
						glUniform1i(4, 1);
					}
					if (objs[i].elements[j].roughnessTexId != -1)
					{
						glActiveTexture(GL_TEXTURE0 + 2);
						glBindTexture(GL_TEXTURE_2D, objs[i].elements[j].roughnessTexId);
						glUniform1i(5, 2);
					}
					if (objs[i].elements[j].metallicTexId != -1)
					{
						glActiveTexture(GL_TEXTURE0 + 3);
						glBindTexture(GL_TEXTURE_2D, objs[i].elements[j].metallicTexId);
						glUniform1i(6, 3);
					}

					glBindVertexArray(objs[i].elements[j].vao);
					glDrawArrays(GL_TRIANGLES, 0, 3 * objs[i].elements[j].numTriangles);
				}
			}
		}
	}
	else
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(quadShader);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, frameTex);
		glUniform1i(0, 0);
		glBindVertexArray(quadVao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // draw quad
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	DrawGui();

	glfwSwapBuffers(window);
}

void Idle()
{
	if (execAfterSave)
	{
		AfterSaving();
		execAfterSave = false;
	}

	if (cameraMouseControl)
	{
		float rotateSpeedFactor = 0.5f;

		double curPosX, curPosY;
		glfwGetCursorPos(window, &curPosX, &curPosY);

		float offsetX = curPosX - curPosDownX;
		float offsetY = curPosY - curPosDownY;
		glm::vec3 offset = glm::vec3(offsetY, offsetX, 0.0f) * rotateSpeedFactor;
		
		previewer.RotateCamera(camRotDown + offset);

		float frameDelta = 1.0f / ImGui::GetIO().Framerate; // in second
		float moveDelta = moveSpeed * moveSpeed * frameDelta;

		glm::vec3 camPos = previewer.CameraPosition();
		glm::vec3 camDir = previewer.CameraDirection();
		glm::vec3 camRight = glm::normalize(glm::cross(previewer.CameraUp(), camDir));
		if (camMoveForward)
			camPos += camDir * moveDelta;
		if (camMoveBackward)
			camPos -= camDir * moveDelta;
		if (camMoveLeft)
			camPos -= camRight * moveDelta;
		if (camMoveRight)
			camPos += camRight * moveDelta;
		
		previewer.SetCamera(camPos, previewer.CameraDirection(), previewer.CameraUp());

		sceneModified = true;
	}

	Display();

	// Only refresh result image on rendering
	/*if (((init || stop || isPausing) && !render) || preview)
		return;

	glBindTexture(GL_TEXTURE_2D, frameTex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, wRender, hRender, GL_RGB, GL_UNSIGNED_BYTE, texData);
	glBindTexture(GL_TEXTURE_2D, 0);*/
}

void Reshape(GLFWwindow* window, int w, int h)
{
	wWindow = w;
	hWindow = h;
	Display();
}

void Kbd(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS && !editingInput)
	{
		switch (key)
		{
		case GLFW_KEY_N:
			if (mods == GLFW_MOD_CONTROL) // CTRL+N = New
				NewScene();
			break;
		case GLFW_KEY_O:
			if (mods == GLFW_MOD_CONTROL) // CTRL+O = Open
				OpenScene();
			break;

		case GLFW_KEY_S:
			if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT)) // CTRL+SHIFT+S = Save As
				SaveAs();
			else if (mods == GLFW_MOD_CONTROL) // CTRL+S = Save
			{
				if (sceneFilePath.size() == 0)
					SaveAs();
				else
					Save();
			}

			else if (mods == GLFW_MOD_SHIFT && canStop) // SHIFT+S = Stop
			{
				render = false;
				restart = false;
				pause = false;
				isPausing = false;
				pathTracer.Exit();
				glfwSetTime(timePause);
				stop = true;
			}

			break;

		case GLFW_KEY_L:
			if (mods == GLFW_MOD_CONTROL && canLoad) // CTRL+L = Load Object
				LoadObject();
			break;

		case GLFW_KEY_E:
			if (mods == GLFW_MOD_CONTROL) // CTRL+E = Export
				Export();
			break;

		case GLFW_KEY_1:
			if (mods == GLFW_MOD_SHIFT) // SHIFT+1 = Preview Mode
				preview = true;
			break;
		case GLFW_KEY_2:
			if (mods == GLFW_MOD_SHIFT) // SHIFT+2 = Path Tracer Output
				preview = false;
			break;

		case GLFW_KEY_B:
			if (mods == GLFW_MOD_SHIFT && canStart) // SHIFT+B = Start
			{
				if (restart || init || stop)
				{
					init = true;
					InitializeFrame();
				}
				render = true;
				preview = false;
				glfwSetTime(timePause);
			}
			break;
		case GLFW_KEY_P:
			if (mods == GLFW_MOD_SHIFT && canPause) // SHIFT+P = Pause
			{
				render = false;
				pause = true;
			}
			break;
		case GLFW_KEY_R:
			if (mods == GLFW_MOD_SHIFT && canRestart) // SHIFT+R = Restart
			{
				render = true;
				preview = false;
				restart = true;
				pathTracer.Exit();
			}

			else if (mods == GLFW_MOD_CONTROL) // CTRL+R = Replace With
			{
				if (previewer.GetNumSelectedObjects() > 0)
					ReplaceWith();
			}

			break;

		case GLFW_KEY_A:
			if (mods == GLFW_MOD_CONTROL) // CTRL+A = Select All
			{
				for (int i = 0; i < previewer.GetLoadedObjects().size(); i++)
					previewer.SelectObject(i, true);
			}
			break;

		case GLFW_KEY_DELETE: // DELETE = Delete Selected
			lastSelectedId = -1;
			previewer.DeleteSelectedObjects();
			break;

		case GLFW_KEY_F5:
			if (canStart) // F5 = Start
			{
				if (restart || init || stop)
				{
					init = true;
					InitializeFrame();
				}
				render = true;
				preview = false;
				glfwSetTime(timePause);
			}
			break;
		case GLFW_KEY_F6:
			if (canPause) // F6 = Pause
			{
				render = false;
				pause = true;
			}
			break;
		case GLFW_KEY_F7:
			if (canStop) // F7 = Stop
			{
				render = false;
				restart = false;
				pause = false;
				isPausing = false;
				pathTracer.Exit();
				glfwSetTime(timePause);
				stop = true;
			}
			break;
		case GLFW_KEY_F8:
			if (canRestart) // F8 = Restart
			{
				render = true;
				preview = false;
				restart = true;
				pathTracer.Exit();
			}
			break;
		}
	}

	if (cameraMouseControl)
	{
		switch (key)
		{
		case GLFW_KEY_W:
			if (action == GLFW_PRESS)
				camMoveForward = true;
			else if (action == GLFW_RELEASE)
				camMoveForward = false;
			break;
		case GLFW_KEY_S:
			if (action == GLFW_PRESS)
				camMoveBackward = true;
			else if (action == GLFW_RELEASE)
				camMoveBackward = false;
			break;
		case GLFW_KEY_A:
			if (action == GLFW_PRESS)
				camMoveLeft = true;
			else if (action == GLFW_RELEASE)
				camMoveLeft = false;
			break;
		case GLFW_KEY_D:
			if (action == GLFW_PRESS)
				camMoveRight = true;
			else if (action == GLFW_RELEASE)
				camMoveRight = false;
			break;
		}
	}
}

void Mouse(GLFWwindow* window, int button, int action, int mods)
{
	// Left click: pick element
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS &&
		preview && canLoad && viewportFocused)
	{
		double curPosX, curPosY;
		glfwGetCursorPos(window, &curPosX, &curPosY);
		glm::ivec2 curPos = glm::ivec2(curPosX, curPosY - menuHeight - toolbarHeight);

		glm::ivec2 viewportSize = glm::ivec2(wWindow - rightBarWidth,
			hWindow - menuHeight - toolbarHeight - statusBarHeight);

		GLint posX = wRender * ((float)curPos.x / (float)viewportSize.x);
		GLint posY = hRender * ((float)curPos.y / (float)viewportSize.y);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		float data[3];
		glReadPixels(posX, hRender - 1 - posY, 1, 1, GL_RGB, GL_FLOAT, &data);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		pickedObjId = data[0] - 1;
		pickedElementId = data[1] - 1;
	}

	// Right press: enter camera mouse control
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && preview && canLoad)
	{
		if (action == GLFW_PRESS)
		{
			double curPosX, curPosY;
			glfwGetCursorPos(window, &curPosX, &curPosY);

			if (viewportFocused)
			{
				cameraMouseControl = true;
				curPosDownX = curPosX;
				curPosDownY = curPosY;
				camRotDown = previewer.CameraRotation();
			}
		}

		else if (action == GLFW_RELEASE)
		{
			if (cameraMouseControl)
			{
				cameraMouseControl = false;
				camMoveForward = false;
				camMoveBackward = false;
				camMoveLeft = false;
				camMoveRight = false;
			}
		}
	}
}

void DropFiles(GLFWwindow* window, int count, const char** paths)
{
	if (!canLoad)
		return;

	for (int i = 0; i < count; i++)
	{
		std::string filename = paths[i];
		filename = PathUtil::UniversalPath(filename);
		std::string ext = filename.substr(filename.find_last_of('.'));
		if (ext == ".pts")
		{
			if (sceneModified)
			{
				showSaveDialog = true;
				pendingLoadSceneFile = filename;
				saveAndLoad = true;
			}
			else
				LoadObjectPathsFromSceneFile(filename);
		}
		else if (ext == ".obj")
		{
			previewer.LoadObject(filename);
			preview = true;
			sceneModified = true;

			statusText = "Loaded object: " + filename;
			statusShowBegin = std::chrono::steady_clock::now();
		}
	}
}

void Close(GLFWwindow* window)
{
	if (sceneModified)
	{
		saveAndExit = true;
		showSaveDialog = true;
		glfwSetWindowShouldClose(window, GLFW_FALSE);
	}
	else
	{
		threadRuning = false;
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}
/* ----- GLFW FUNCTIONS ------ */

/* ----- PROGRAM FUNCTIONS ------ */
void InitializeProgram(GLuint* program, const std::string& vert, const std::string& frag)
{
	std::vector<GLuint> shaderList;

	// load and compile shaders 	
	shaderList.push_back(CreateShader(GL_VERTEX_SHADER, vert));
	shaderList.push_back(CreateShader(GL_FRAGMENT_SHADER, frag));

	// create the shader program and attach the shaders to it
	*program = CreateProgram(shaderList);

	// delete shaders (they are on the GPU now)
	std::for_each(shaderList.begin(), shaderList.end(), glDeleteShader);
}

int InitializeGL(GLFWwindow*& window)
{
	if (!glfwInit())
		return -1;
	
	wWindow = wRender + rightBarWidth;
	hWindow = hRender + menuHeight + toolbarHeight + statusBarHeight;
	window = glfwCreateWindow(wWindow, hWindow, "PathTracer", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	computeWindow = glfwCreateWindow(1, 1, "Compute", NULL, window);
	if (!computeWindow)
	{
		glfwTerminate();
		return -1;
	}
	glfwHideWindow(computeWindow);

	// Load window icons
	int iconWidth, iconHeight, iconChannels;
	GLFWimage windowIcons[4]{};

	unsigned char* icon256 = stbi_load_from_memory(_acicon_256, 17238UL + 1,
		&iconWidth, &iconHeight, &iconChannels, 4);
	windowIcons[0].width = iconWidth;
	windowIcons[0].height = iconHeight;
	windowIcons[0].pixels = icon256;

	unsigned char* icon48 = stbi_load_from_memory(_acicon_48, 2402UL + 1,
		&iconWidth, &iconHeight, &iconChannels, 4);
	windowIcons[1].width = iconWidth;
	windowIcons[1].height = iconHeight;
	windowIcons[1].pixels = icon48;

	unsigned char* icon32 = stbi_load_from_memory(_acicon_32, 1505UL + 1,
		&iconWidth, &iconHeight, &iconChannels, 4);
	windowIcons[2].width = iconWidth;
	windowIcons[2].height = iconHeight;
	windowIcons[2].pixels = icon32;

	unsigned char* icon16 = stbi_load_from_memory(_acicon_16, 680UL + 1,
		&iconWidth, &iconHeight, &iconChannels, 4);
	windowIcons[3].width = iconWidth;
	windowIcons[3].height = iconHeight;
	windowIcons[3].pixels = icon16;

	glfwSetWindowIcon(window, 4, windowIcons);

	glfwSetKeyCallback(window, Kbd);
	glfwSetMouseButtonCallback(window, Mouse);
	glfwSetFramebufferSizeCallback(window, Reshape);
	glfwSetDropCallback(window, DropFiles);
	glfwSetWindowCloseCallback(window, Close);

	glfwMakeContextCurrent(window);

	glewInit();

	InitializeProgram(&quadShader, vQuad, fQuad);
	InitializeProgram(&previewShader, vPrev, fPrev);

	glEnable(GL_DEPTH_TEST);

	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_FILL);

	glGenTextures(1, &appIconTex);
	glBindTexture(GL_TEXTURE_2D, appIconTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		windowIcons[0].width, windowIcons[0].height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, icon256);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(icon256);
	stbi_image_free(icon48);
	stbi_image_free(icon32);
	stbi_image_free(icon16);

	return 0;
}

void InitializeFrame()
{
	if (quadVao == -1)
		glGenVertexArrays(1, &quadVao);

	if (frameTex == -1)
		glGenTextures(1, &frameTex);
	glBindTexture(GL_TEXTURE_2D, frameTex);
	if (texData)
		delete[] texData;
	texData = new GLubyte[wRender * hRender * 3];
	for (int i = 0; i < wRender * hRender * 3; i++)
		texData[i] = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, wRender, hRender, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void InitializeGLFrame()
{
	if (fboTex == -1)
		glGenTextures(1, &fboTex);
	glBindTexture(GL_TEXTURE_2D, fboTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wRender, hRender, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (pickTex == -1)
		glGenTextures(1, &pickTex);
	glBindTexture(GL_TEXTURE_2D, pickTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, wRender, hRender, 0, GL_RGB, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (rbo == -1)
		glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, wRender, hRender);
	if (fbo == -1)
	{
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pickTex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glUseProgram(previewShader);
	if (previewMaterialUBO != -1)
		glDeleteBuffers(1, &previewMaterialUBO);
	glGenBuffers(1, &previewMaterialUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, previewMaterialUBO);
	glBufferData(GL_UNIFORM_BUFFER, 56, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void InitializeImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	ImGui::GetIO().IniFilename = 0;

	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(SourceSansPro_compressed_data,
		SourceSansPro_compressed_size, 17);

	static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;

	normalIconFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(ForkAwesome_compressed_data,
		ForkAwesome_compressed_size, 14, &icons_config, icons_ranges);

	ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(SourceSansPro_compressed_data,
		SourceSansPro_compressed_size, 17);
	icons_config.GlyphOffset.y += (22 - 17) * 0.5f;
	bigIconFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(ForkAwesome_compressed_data,
		ForkAwesome_compressed_size, 22, &icons_config, icons_ranges);

	ImGui::GetIO().FontDefault = bigIconFont;

	ImGui::GetStyle().WindowPadding = ImVec2(6, 4);
	ImGui::GetStyle().FramePadding = ImVec2(8, 2);
	ImGui::GetStyle().ItemSpacing = ImVec2(4, 6);
	ImGui::GetStyle().IndentSpacing = 12;
	ImGui::GetStyle().FrameBorderSize = 1;
	ImGui::GetStyle().FrameRounding = 3;
	ImGui::GetStyle().GrabRounding = 2;

	ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_PopupBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.4f);
	ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4f, 0.4f, 0.4f, 0.4f);
	ImGui::GetStyle().Colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = ImVec4(0.1f, 0.4f, 0.9f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_Tab] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_TabHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	ImGui::GetStyle().Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	ImGui::GetStyle().Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	ImGui::GetStyle().Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	ImGui::GetStyle().Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
}

void GlfwLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwMakeContextCurrent(window);
		Idle();
		glfwPollEvents();
	}
}

void InitializePathTracer()
{
	glfwMakeContextCurrent(computeWindow);

	std::vector<GLuint> shaderList;
	shaderList.push_back(CreateShader(GL_COMPUTE_SHADER, compPt));
	computeProgram = CreateProgram(shaderList);
	std::for_each(shaderList.begin(), shaderList.end(), glDeleteShader);
	pathTracer.GPUSetProgram(computeProgram);
	pathTracer.GPUSetOutImage(frameTex);
}

void PathTracerLoop()
{
	while (threadRuning)
	{
		if (render)
		{
			glfwMakeContextCurrent(computeWindow);
			isPausing = false;
			if (restart || init || stop)
			{
				pathTracer.ClearScene();
				pathTracer.GPUClearScene();
				previewer.SendObjectsToPathTracer(&pathTracer);
				previewer.SetPathTracerCamera(&pathTracer);

				restart = false;
				stop = false;
				pathTracer.SetResolution(glm::ivec2(wRender, hRender));
				pathTracer.SetTraceDepth(traceDepth);
				//pathTracer.SetOutImage(texData);
				pathTracer.ResetImage();
				pathTracer.GPUBuildScene();

				glfwSetTime(0.0); // reset counter

				init = false;
			}
			pathTracer.GPURenderFrame();
		}
		if (pause)
		{
			timePause = glfwGetTime();
			pause = false;
			isPausing = true;
		}
		if (stop)
		{
			pathTracer.ClearScene();
			pathTracer.GPUClearScene();
		}

		if (exportFile)
		{
			ExportAt(exportFilePath);
			exportFile = false;
		}

		if (saveFile)
		{
			SaveAt(sceneFilePath);
			saveFile = false;
			sceneModified = false;
			execAfterSave = true;
		}

		if (render && pathTracer.GetSamples() == targetSample)
		{
			render = false;
			pause = true;
		}
	}
}

void OnExit()
{
	if (texData)
		delete[] texData;

	if (quadVao != -1)
		glDeleteVertexArrays(1, &quadVao);
	if (rbo != -1)
		glDeleteRenderbuffers(1, &rbo);
	if (fbo != -1)
		glDeleteFramebuffers(1, &fbo);
	if (previewMaterialUBO != -1)
		glDeleteBuffers(1, &previewMaterialUBO);
	if (frameTex != -1)
		glDeleteTextures(1, &frameTex);
	if (fboTex != -1)
		glDeleteTextures(1, &fboTex);
	if (pickTex != 1)
		glDeleteTextures(1, &pickTex);
	if (appIconTex != -1)
		glDeleteTextures(1, &appIconTex);

	if (quadShader != -1)
		glDeleteProgram(quadShader);
	if (previewShader != -1)
		glDeleteProgram(previewShader);
	if (computeProgram != -1)
		glDeleteProgram(computeProgram);
}
/* ----- PROGRAM FUNCTIONS ------ */

int main(int argc, char** argv)
{
	if (argc > 1)
		GetResolutionFromSceneFile(PathUtil::UniversalPath(argv[1]));

	int initRes = InitializeGL(window);
	if (initRes)
		return initRes;

	if (argc > 1)
		LoadObjectPathsFromSceneFile(PathUtil::UniversalPath(argv[1]));

	InitializeImGui();
	InitializeGLFrame();
	InitializeFrame();

	InitializePathTracer();

	std::thread computeThread(PathTracerLoop);
	GlfwLoop();
	computeThread.join();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	OnExit();
	return 0;
}
