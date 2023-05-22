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
	int id;

	MaterialType type;
	glm::vec3 diffuse;
	glm::vec3 specular;
	glm::vec3 emissive;

	float emissiveIntensity;
	float roughness;
	float reflectiveness;
	float translucency;
	float ior;

	int diffuseTexId;
	int normalTexId;
	int emissTexId;
	int roughnessTexId;
	int metallicTexId;
	int opacityTexId;

	Material() :
		id(-1),
		type(MaterialType::OPAQUE),
		emissiveIntensity(1.0f),
		roughness(1.0f),
		reflectiveness(0.0f),
		translucency(1.0f),
		ior(1.5f),
		diffuseTexId(-1),
		normalTexId(-1),
		emissTexId(-1),
		roughnessTexId(-1),
		metallicTexId(-1),
		opacityTexId(-1)
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
	int id = -1;

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

	Material material;

	void Init();
};

struct GPUMaterial
{
	glm::vec4 diffuse;            // 16 bytes
	glm::vec4 specular;           // 16 bytes
	glm::vec4 emissive;           // 16 bytes

	float emissIntensity = 0.f;    // 4 bytes
	float roughness = 0.f;         // 4 bytes
	float reflectiveness = 0.f;    // 4 bytes
	float translucency = 1.0f;		// 4 bytes

	float ior = 0.f;               // 4 bytes
	int translucent = 0;         // 4 bytes
	int diffuseTexId = -1;		// 4 bytes
	int normalTexId = -1;         // 4 bytes

	int emissTexId = -1;		// 4 bytes
	int roughnessTexId = -1;	// 4 bytes
	int metallicTexId = -1;		// 4 bytes
	int opacityTexId = -1;		// 4 bytes
};

struct GPUTriangle
{
	glm::vec4 v1;                 // 16 bytes
	glm::vec4 v2;                 // 16 bytes
	glm::vec4 v3;                 // 16 bytes
	glm::vec4 n1;                 // 16 bytes
	glm::vec4 n2;                 // 16 bytes
	glm::vec4 n3;                 // 16 bytes
	glm::vec4 normal;             // 16 bytes
	glm::vec4 tangent;            // 16 bytes
	glm::vec4 bitangent;          // 16 bytes

	glm::vec2 uv1;                // 8 bytes
	glm::vec2 uv2;                // 8 bytes

	glm::vec2 uv3;                // 8 bytes
	int objectId = -1;				// 4 bytes
	int elementId = -1;				// 4 bytes

	glm::vec4 barycentric_v0;     // 16 bytes
	glm::vec4 barycentric_v1;     // 16 bytes

	float barycentric_d00 = 0.f;   // 4 bytes
	float barycentric_d01 = 0.f;   // 4 bytes
	float barycentric_d11 = 0.f;   // 4 bytes
	float barycentric_invDenom = 0.f;  // 4 bytes

	int smoothing = 0;            // 4 bytes
	int id = -1;					// 4 bytes
	int materialId = -1;			// 4 bytes
	int padding;					// 4 bytes
};

struct GPUBVHNode
{
	int nodeIndex = -1;               // 4 bytes
	int rightOffset = -1;             // 4 bytes
	int triangleId = -1;			// 4 bytes
	int padding = 0;				// 4 bytes

	glm::vec4 aabb_min;               // 16 bytes
	glm::vec4 aabb_max;               // 16 bytes
};

class BVHNode
{
private:
	AABB mBox;
	int mTriangleId;
	BVHNode* mLeft;
	BVHNode* mRight;

	std::uniform_int_distribution<int> mRandAxis;
	static bool TriXCompare(const GPUTriangle& a, const GPUTriangle& b);
	static bool TriYCompare(const GPUTriangle& a, const GPUTriangle& b);
	static bool TriZCompare(const GPUTriangle& a, const GPUTriangle& b);

	const bool IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b);
	const bool IsInside(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

public:
	BVHNode();
	BVHNode(const GPUTriangle& t);
	~BVHNode();
	BVHNode* Construct(std::vector<GPUTriangle>& triangles, int size, int offset = 0);

	void GetGPULayout(std::vector<GPUBVHNode>& bvh);
};

#endif
