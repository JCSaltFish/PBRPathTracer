#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>

#include "image.h"

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

enum class MaterialType
{
	OPAQUE,
	TRANSLUCENT
};

struct Material
{
	MaterialType type;
	glm::vec3 diffuse;
	glm::vec3 specular;
	glm::vec3 emissive;

	float emissiveIntensity;
	float roughness;
	float reflectiveness;
	float translucency;
	float ior;

	Image* diffuseTex;
	Image* normalTex;
	Image* emissTex;
	Image* roughnessTex;
	Image* metallicTex;
	Image* opacityTex;

	Material() :
		type(MaterialType::OPAQUE),
		emissiveIntensity(1.0f),
		roughness(1.0f),
		reflectiveness(0.0f),
		translucency(1.0f),
		ior(1.5f),
		diffuseTex(0),
		normalTex(0),
		emissTex(0),
		roughnessTex(0),
		metallicTex(0),
		opacityTex(0)
	{
		diffuse = glm::vec3(1.0f);
		specular = glm::vec3(1.0f);
		emissive = glm::vec3(0.0f);
	}
};

struct AABB
{
	glm::vec3 min = glm::vec3(INF);
	glm::vec3 max = glm::vec3(-INF);

	void Build(const glm::vec3& v);
	void Check();
	bool Intersect(const glm::vec3& ro, const glm::vec3& rd);
};

struct TriangleBarycentricInfo
{
	glm::vec3 v0;
	glm::vec3 v1;
	float d00 = 0.0f;
	float d01 = 0.0f;
	float d11 = 0.0f;
	float invDenom = 0.0f;
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

	bool smoothing = false;

	int objectId = -1;
	int elementId = -1;
	Material* mat;

	void Init();
};

class BVHNode
{
public:
	AABB mBox;
	Triangle* mTriangle;
	BVHNode* mLeft;
	BVHNode* mRight;

private:
	std::uniform_int_distribution<int> mRandAxis;
	static bool TriXCompare(const Triangle& a, const Triangle& b);
	static bool TriYCompare(const Triangle& a, const Triangle& b);
	static bool TriZCompare(const Triangle& a, const Triangle& b);

public:
	BVHNode();
	BVHNode(Triangle* t);
	~BVHNode();
	BVHNode* Construct(std::vector<Triangle>& triangles, int size, int offset = 0);
};

#endif
