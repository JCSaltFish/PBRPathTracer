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
		bool isVisible;

		Element()
		{
			name = "";
			isVisible = true;
		}

		Element(const std::string& name)
		{
			this->name = name;
			isVisible = true;
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
	std::vector<GPUTriangle> mTriangles;
	BVHNode* mBvh;

	std::vector<PathTracerLoader::Object> mLoadedObjects;
	std::vector<GLuint> mTextures;

	glm::ivec2 mResolution;
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

	GLuint m_GPUOutImage;
	GLuint m_GPUProgram;

	GLuint m_GPUTrianglesSSBO;
	GLuint m_GPUMaterialsSSBO;
	GLuint m_GPUBVHSSBO;
	GLuint m_GPULightsourcesSSBO;
	GLuint m_GPUTexSizesSSBO;

	GLuint m_GPUTextures;

	GLuint m_GPUPathTracerUBO;
	struct GPUPathTracerUniforms
	{
		glm::ivec2 resolution;
		int maxDepth = 3;
		int samples = 0;
	};
	GLuint m_GPUCameraUBO;
	struct GPUCameraUniforms
	{
		glm::vec4 camPos;
		glm::vec4 camDir;
		glm::vec4 camUp;
		float camFocal = 0.05f;
		float camFovy = 90.0f;
		float camFocalDist = 5.0f;
		float camAperture = 0.0f;
	};

public:
	PathTracer();
	~PathTracer();

public:
	void LoadObject(const std::string& file, const glm::mat4& model);

	void SetDiffuseTextureForElement(int objId, int elementId, GLuint texture);
	void SetNormalTextureForElement(int objId, int elementId, GLuint texture);
	void SetEmissTextureForElement(int objId, int elementId, GLuint texture);
	void SetRoughnessTextureForElement(int objId, int elementId, GLuint texture);
	void SetMetallicTextureForElement(int objId, int elementId, GLuint texture);
	void SetOpacityTextureForElement(int objId, int elementId, GLuint texture);

	void SetMaterial(int objId, int elementId, Material& material);

	void BuildBVH();
	void ResetImage();
	void ClearScene();

	const int GetSamples() const;
	const int GetTriangleCount() const;
	const int GetTraceDepth() const;
	void SetTraceDepth(int depth);
	void SetResolution(const glm::ivec2& res);
	const glm::ivec2 GetResolution() const;
	std::vector<PathTracerLoader::Object> GetLoadedObjects() const;

	void SetCamera(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up);
	void SetProjection(float f, float fovy);
	void SetCameraFocalDist(float dist);
	void SetCameraAperture(float aperture);
	void Exit();

	void GPUSetOutImage(GLuint texture);
	void GPUSetProgram(GLuint program);
	void GPUBuildScene();
	void GPURenderFrame();
	void GPUClearScene();
};

#endif
