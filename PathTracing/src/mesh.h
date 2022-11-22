#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include <random>
#include <algorithm>

#include <cuda_runtime.h>

#include <glm/glm.hpp>

#include "image.h"

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

enum class MaterialType
{
	DIFFUSE,
	SPECULAR,
	GLOSSY,
	GLASS
};

struct Material
{
	MaterialType type;
	glm::vec3 baseColor;
	float roughness;
	glm::vec3 emissive;
	float emissiveIntensity;

	int normalTexId;

	__host__ __device__ Material() :
		type(MaterialType::DIFFUSE),
		roughness(0.0f),
		emissiveIntensity(1.0f),
		normalTexId(-1)
	{
		baseColor = glm::vec3(1.0f);
		emissive = glm::vec3(0.0f);
	}
};

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;

	__host__ __device__ AABB()
	{
		min = glm::vec3(INF);
		max = glm::vec3(-INF);
	}

	void Build(const glm::vec3& v);
	void Check();
	bool Intersect(const glm::vec3& ro, const glm::vec3& rd);
};

struct TriangleBarycentricInfo
{
	glm::vec3 v0;
	glm::vec3 v1;
	float d00;
	float d01;
	float d11;
	float invDenom;

	__host__ __device__ TriangleBarycentricInfo() :
		d00(0.0f),
		d01(0.0f),
		d11(0.0f),
		invDenom(0.0f)
	{
		v0 = glm::vec3();
		v1 = glm::vec3();
	}
};

struct Triangle
{
	glm::vec3 v1;
	glm::vec3 v2;
	glm::vec3 v3;

	glm::vec3 n1;
	glm::vec3 n2;
	glm::vec3 n3;

	glm::vec2 uv1;
	glm::vec2 uv2;
	glm::vec2 uv3;

	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	TriangleBarycentricInfo barycentricInfo;

	bool smoothing;

	int objectId;
	int elementId;

	// For GPU indexing
	Material material;

	__host__ __device__ Triangle() :
		smoothing(false),
		objectId(-1),
		elementId(-1)
	{
		v1 = glm::vec3();
		v2 = glm::vec3();
		v3 = glm::vec3();

		n1 = glm::vec3(1.0f, 0.0f, 0.0f);
		n2 = glm::vec3(1.0f, 0.0f, 0.0f);
		n3 = glm::vec3(1.0f, 0.0f, 0.0f);

		uv1 = glm::vec2();
		uv2 = glm::vec2();
		uv3 = glm::vec2();

		normal = glm::vec3(1.0f, 0.0f, 0.0f);
		tangent = glm::vec3(0.0f, 1.0f, 0.0f);
		bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
	}

	void Init();
};

struct GPUBVHNode
{
	AABB box;
	Triangle triangle;
	int nodeIndex;
	int rightOffset;

	__host__ __device__ GPUBVHNode() :
		nodeIndex(-1),
		rightOffset(-1)
	{
	}
};

class BVHNode
{
private:
	AABB mBox;
	Triangle mTriangle;
	BVHNode* mLeft;
	BVHNode* mRight;

private:
	std::uniform_int_distribution<int> mRandAxis;
	static bool TriXCompare(const Triangle& a, const Triangle& b);
	static bool TriYCompare(const Triangle& a, const Triangle& b);
	static bool TriZCompare(const Triangle& a, const Triangle& b);

private:
	const bool IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b);
	const bool IsInside(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

public:
	BVHNode();
	BVHNode(const Triangle& t);
	~BVHNode();
	BVHNode* Construct(std::vector<Triangle>& triangles);
	const bool Hit(const glm::vec3& ro, const glm::vec3& rd, Triangle& triangleOut, float& distOut);

	void GetGPULayout(std::vector<GPUBVHNode>& bvh);
};

#endif
