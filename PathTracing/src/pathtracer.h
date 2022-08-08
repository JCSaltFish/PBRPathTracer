#ifndef __PATHTRACER_H__
#define __PATHTRACER_H__

#include <string>
#include <vector>
#include <random>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "mesh.h"

enum class MaterialType
{
	DIFFUSE,
	SPECULAR,
	GLOSSY,
	GLASS
};

struct Material
{
	MaterialType type = MaterialType::DIFFUSE;
	glm::vec3 baseColor = glm::vec3(1.0f);
	float roughness = 0.0f;
	glm::vec3 emissive = glm::vec3(0.0f);
	float emissiveIntensity = 1.0f;

	int normalTexId = -1;
};

namespace PathTracerLoader
{
	struct Element
	{
		std::string name;
		Material material;

		Element()
		{
			name = "";
		}

		Element(std::string name)
		{
			this->name = name;
		}
	};

	struct Object
	{
		std::string name;
		std::vector<Element> elements;

		Object()
		{
			name = "";
		}

		Object(std::string name)
		{
			this->name = name;
		}
	};
}

class PathTracer
{
private:
	std::vector<Triangle> mTriangles;
	BVHNode* mBvh;

	std::vector<PathTracerLoader::Object> mLoadedObjects;
	std::vector<Image*> mLoadedTextures;

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
	bool mExit;

	std::mt19937 mRng;

public:
	PathTracer();
	~PathTracer();

private:
	float Rand();
	glm::vec2 GetUV(glm::vec3& p, Triangle& t);
	glm::vec3 GetSmoothNormal(glm::vec3& p, Triangle& t);
	glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd, int depth = 0, bool inside = false);

public:
	void LoadObject(std::string file, glm::mat4 model);
	void SetNormalTextureForElement(int objId, int elementId, std::string file);
	void SetMaterial(int objId, int elementId, Material& material);
	void BuildBVH();
	void ResetImage();
	void ClearScene();

	int GetSamples();
	int GetTriangleCount();
	int GetTraceDepth();
	void SetTraceDepth(int depth);
	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();
	std::vector<PathTracerLoader::Object> GetLoadedObjects();

	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
	void Exit();
};

#endif
