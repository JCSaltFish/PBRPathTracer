#ifndef __PATHTRACER_H__brdf.h
#define __PATHTRACER_H__

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "brdf.h"

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

struct MeshTriangle
{
	glm::vec3 v1;
	glm::vec3 v2;
	glm::vec3 v3;
	glm::vec3 normal;
};

struct Mesh
{
    Material material;
    std::vector<MeshTriangle> triangles;
};

class PathTracer
{
private:
    std::vector<Mesh> scene_mesh;
    std::vector<Light> scene_lights; // or store lights in list

	glm::ivec2 m_imgResolution;
	GLubyte* m_outImg;

	glm::vec3 m_camPos;
	glm::vec3 m_camDir;
	glm::vec3 m_camUp;
	float m_camFocal;
	float m_camFovy;

	int samples;

public:
	PathTracer();
	~PathTracer();

private:
	bool Intersect(glm::vec3 ro, glm::vec3 rd, MeshTriangle t, float& distOut);
	glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd, int depth = 0);

public:
	int LoadMesh(std::string file, glm::mat4 model);
	void SetMeshMaterial(int id, Material m);
    void AddLight(glm::vec3 position, glm::vec3 color);

	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();
	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
};

#endif
