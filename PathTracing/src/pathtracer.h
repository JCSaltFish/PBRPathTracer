#ifndef __PATHTRACER_H__
#define __PATHTRACER_H__

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

struct MeshTriangle
{
	glm::vec3 v1;
	glm::vec3 v2;
	glm::vec3 v3;
	glm::vec3 normal;
};

class PathTracer
{
private:
	std::vector<MeshTriangle> m_scene;

	glm::ivec2 m_imgResolution;
	GLubyte* m_outImg;

	glm::vec3 m_camPos;
	glm::vec3 m_camDir;
	glm::vec3 m_camUp;
	float m_camFocal;
	float m_camFovy;

public:
	PathTracer();
	~PathTracer();

private:
	bool Intersect(glm::vec3 ro, glm::vec3 rd, MeshTriangle t, float& distOut);
	glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd);

public:
	void LoadMesh(std::string file, glm::mat4 model);

	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();
	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
};

#endif
