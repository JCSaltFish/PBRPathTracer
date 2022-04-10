#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include <random>
#include <algorithm>
#include <glm/glm.hpp>

const float EPS = 0.001f;
const float INF = (float)0xFFFF;

struct AABB
{
	glm::vec3 min = glm::vec3(INF);
	glm::vec3 max = glm::vec3(-INF);

	void Build(glm::vec3 v);
	void Check();
	bool Intersect(glm::vec3 ro, glm::vec3 rd);
};

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
	glm::vec3 baseColor = glm::vec3(0.0f);
	float roughness = 0.0f;
	glm::vec3 emissive = glm::vec3(0.0f);
};

struct Triangle
{
	glm::vec3 v1;
	glm::vec3 v2;
	glm::vec3 v3;
	glm::vec3 normal;
	Material material;
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

    bool IsSameSide(glm::vec3 p1, glm::vec3 p2, glm::vec3 a, glm::vec3 b);
    bool IsInside(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c);

public:
	BVHNode();
	BVHNode(Triangle t);
	~BVHNode();
	BVHNode* Construct(std::vector<Triangle> triangles);
	bool Hit(glm::vec3 ro, glm::vec3 rd, Triangle& triangleOut, float& distOut);
};

#endif
