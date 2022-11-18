#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include <random>
#include <algorithm>

#include <glm/glm.hpp>

#include "image.h"

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

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

	void Init();
};

class BVHNode
{
private:
	AABB mBox;
	Triangle mTriangle;
	BVHNode* mLeft;
	BVHNode* mRight;

	std::uniform_int_distribution<int> mRandAxis;
	static bool TriXCompare(const Triangle& a, const Triangle& b);
	static bool TriYCompare(const Triangle& a, const Triangle& b);
	static bool TriZCompare(const Triangle& a, const Triangle& b);

	const bool IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b);
	const bool IsInside(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

public:
	BVHNode();
	BVHNode(const Triangle& t);
	~BVHNode();
	BVHNode* Construct(std::vector<Triangle>& triangles);
	const bool Hit(const glm::vec3& ro, const glm::vec3& rd, Triangle& triangleOut, float& distOut);
};

#endif
