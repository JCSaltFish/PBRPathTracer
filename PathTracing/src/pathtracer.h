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

struct AABB
{
	glm::vec3 min = glm::vec3(INF);
	glm::vec3 max = glm::vec3(-INF);

	bool Intersect(glm::vec3 ro, glm::vec3 rd);
};

struct Mesh
{
    Material material;
    std::vector<MeshTriangle> triangles;
	AABB aabb;
};

class PathTracer
{
private:
    std::vector<Mesh> scene_mesh;

	glm::ivec2 m_imgResolution;
	GLubyte* m_outImg;

	glm::vec3 m_camPos;
	glm::vec3 m_camDir;
	glm::vec3 m_camUp;
	float m_camFocal;
	float m_camFovy;

	int samples;
	int seed;

public:
	PathTracer();
	~PathTracer();

private:
	void BuildAABB(glm::vec3 v, AABB& aabb);

	bool Intersect(glm::vec3 ro, glm::vec3 rd, MeshTriangle t, float& distOut);
	float Rand(glm::vec2 co);
	glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd, glm::vec2 seed, int depth = 0);

public:
	int LoadMesh(std::string file, glm::mat4 model, bool ccw = false);
	void SetMeshMaterial(int id, Material m);
	int GetSamples();

	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();
	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
};

#endif
