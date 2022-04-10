#ifndef __PATHTRACER_H__
#define __PATHTRACER_H__

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "mesh.h"

class PathTracer
{
private:
	std::vector<Triangle> mTriangles;
	BVHNode* mBvh;

	glm::ivec2 mResolution;
	GLubyte* mOutImg;
	float* mTotalImg;
	int mMaxDepth;

	glm::vec3 mCamPos;
	glm::vec3 mCamDir;
	glm::vec3 mCamUp;
	float mCamFocal;
	float mCamFovy;

	int mSamples;
	bool mNeedReset;

public:
	PathTracer();
	~PathTracer();

private:
	float Rand(glm::vec2 co, float& seed);
	glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd, glm::vec2 raySeed, float& randSeed, int depth = 0, bool inside = false);

public:
	void LoadMesh(std::string file, glm::mat4 model, Material material);
	void ResetImage();

	int GetSamples();
	int GetTriangleCount();
	int GetTraceDepth();
	void SetTraceDepth(int depth);
	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();

	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
};

#endif
