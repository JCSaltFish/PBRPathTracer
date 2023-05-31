#ifndef __PATHTRACER_H__
#define __PATHTRACER_H__

#include <string>
#include <vector>
#include <random>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "mesh.h"

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

		Element(const std::string& name)
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

		Object(const std::string& name)
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
	std::vector<Triangle*> mLights;

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
	float mCamFocalDist;
	float mCamAperture;

	int mSamples;
	bool mNeedReset;
	bool mExit;

	std::mt19937 mRng;

public:
	PathTracer();
	~PathTracer();

private:
	const float Rand();
	const glm::vec3 IntersectTriangle
	(
		const glm::vec3& ro, const glm::vec3& rd,
		const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2
	) const;
	const bool Hit
	(
		BVHNode* node, const glm::vec3& ro, const glm::vec3& rd,
		Triangle*& triangleOut, float& distOut, glm::vec2& cOut
	);
	const glm::vec3 SampleTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
	const glm::vec3 DirectIllumimation(const glm::vec3& rd, const glm::vec3& p, const glm::vec3& n, const glm::vec3& diffuse);
	const glm::vec2 GetUV(const glm::vec2& c, Triangle* t) const;
	const glm::vec3 GetSmoothNormal(const glm::vec2& c, Triangle* t) const;
	const glm::vec2 SampleCircle();
	const glm::vec3 Trace(const glm::vec3& ro, const glm::vec3& rd, int depth = 0, int iter = 0, bool inside = false);

public:
	void LoadObject(const std::string& file, const glm::mat4& model);
	
	void SetDiffuseTextureForElement(int objId, int elementId, const std::string& file);
	void SetNormalTextureForElement(int objId, int elementId, const std::string& file);
	void SetEmissTextureForElement(int objId, int elementId, const std::string& file);
	void SetRoughnessTextureForElement(int objId, int elementId, const std::string& file);
	void SetMetallicTextureForElement(int objId, int elementId, const std::string& file);
	void SetOpacityTextureForElement(int objId, int elementId, const std::string& file);

	void SetMaterial(int objId, int elementId, Material& material);

	void BuildBVH();
	void ResetImage();
	void ClearScene();

	const int GetSamples() const;
	const int GetTriangleCount() const;
	const int GetTraceDepth() const;
	void SetTraceDepth(int depth);
	void SetOutImage(GLubyte* out);
	void SetResolution(const glm::ivec2& res);
	const glm::ivec2 GetResolution() const;
	std::vector<PathTracerLoader::Object> GetLoadedObjects() const;

	void SetCamera(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up);
	void SetProjection(float f, float fovy);
	void SetCameraFocalDist(float dist);
	void SetCameraAperture(float aperture);
	void RenderFrame();
	void Exit();
};

#endif
