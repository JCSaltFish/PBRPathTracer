#define _USE_MATH_DEFINES
#include <math.h>

#include "mesh.h"

void AABB::Build(const glm::vec3& v)
{
	float minX = min.x;
	float minY = min.y;
	float minZ = min.z;
	float maxX = max.x;
	float maxY = max.y;
	float maxZ = max.z;

	if (v.x < minX)
		minX = v.x;
	if (v.y < minY)
		minY = v.y;
	if (v.z < minZ)
		minZ = v.z;
	if (v.x > maxX)
		maxX = v.x;
	if (v.y > maxY)
		maxY = v.y;
	if (v.z > maxZ)
		maxZ = v.z;

	min = glm::vec3(minX, minY, minZ);
	max = glm::vec3(maxX, maxY, maxZ);
}

void AABB::Check()
{
	float maxX = max.x;
	float maxY = max.y;
	float maxZ = max.z;

	if (min.x == maxX)
		maxX += EPS;
	if (min.y == maxY)
		maxY += EPS;
	if (min.z == maxZ)
		maxZ += EPS;

	max = glm::vec3(maxX, maxY, maxZ);
}

bool AABB::Intersect(const glm::vec3& ro, const glm::vec3& rd)
{
	glm::vec3 tMin = (min - ro) / rd;
	glm::vec3 tMax = (max - ro) / rd;
	glm::vec3 t1 = glm::min(tMin, tMax);
	glm::vec3 t2 = glm::max(tMin, tMax);
	float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
	float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);
	if (tNear >= tFar)
		return false;
	return true;
}

void Triangle::Init()
{
	// barycentric
	barycentricInfo.v0 = v2 - v1;
	barycentricInfo.v1 = v3 - v1;
	barycentricInfo.d00 = glm::dot(barycentricInfo.v0, barycentricInfo.v0);
	barycentricInfo.d01 = glm::dot(barycentricInfo.v0, barycentricInfo.v1);
	barycentricInfo.d11 = glm::dot(barycentricInfo.v1, barycentricInfo.v1);
	barycentricInfo.invDenom = 1.0f /
		(barycentricInfo.d00 * barycentricInfo.d11 -
			barycentricInfo.d01 * barycentricInfo.d01);

	// TBN
	glm::vec3 e1 = v2 - v1;
	glm::vec3 e2 = v3 - v1;
	glm::vec2 deltaUv1 = uv2 - uv1;
	glm::vec2 deltaUv2 = uv3 - uv1;
	float f = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y);

	tangent.x = f * (deltaUv2.y * e1.x - deltaUv1.y * e2.x);
	tangent.y = f * (deltaUv2.y * e1.y - deltaUv1.y * e2.y);
	tangent.z = f * (deltaUv2.y * e1.z - deltaUv1.y * e2.z);

	bitangent.x = f * (-deltaUv2.x * e1.x + deltaUv1.x * e2.x);
	bitangent.y = f * (-deltaUv2.x * e1.y + deltaUv1.x * e2.y);
	bitangent.z = f * (-deltaUv2.x * e1.z + deltaUv1.x * e2.z);

	normal = glm::cross(e1, e2);

	tangent = glm::normalize(tangent);
	bitangent = glm::normalize(bitangent);
	normal = glm::normalize(normal);
}

BVHNode::BVHNode()
{
	mLeft = 0;
	mRight = 0;
	mTriangle = 0;

	mRandAxis = std::uniform_int_distribution<int>(0, 2);
}

BVHNode::BVHNode(Triangle* t) : mTriangle(t)
{
	mBox.Build(t->v1);
	mBox.Build(t->v2);
	mBox.Build(t->v3);
	mBox.Check();

	mLeft = 0;
	mRight = 0;

	mRandAxis = std::uniform_int_distribution<int>(0, 2);
}

BVHNode::~BVHNode()
{
	if (mLeft)
		delete mLeft;
	if (mRight)
		delete mRight;
}

bool BVHNode::TriXCompare(const Triangle& a, const Triangle& b)
{
	AABB boxA, boxB;
	boxA.Build(a.v1);
	boxA.Build(a.v2);
	boxA.Build(a.v3);
	boxA.Check();
	boxB.Build(b.v1);
	boxB.Build(b.v2);
	boxB.Build(b.v3);
	boxB.Check();

	if (boxA.min.x > boxB.min.x)
		return true;
	else
		return false;
}

bool BVHNode::TriYCompare(const Triangle& a, const Triangle& b)
{
	AABB boxA, boxB;
	boxA.Build(a.v1);
	boxA.Build(a.v2);
	boxA.Build(a.v3);
	boxA.Check();
	boxB.Build(b.v1);
	boxB.Build(b.v2);
	boxB.Build(b.v3);
	boxB.Check();

	if (boxA.min.y > boxB.min.y)
		return true;
	else
		return false;
}

bool BVHNode::TriZCompare(const Triangle& a, const Triangle& b)
{
	AABB boxA, boxB;
	boxA.Build(a.v1);
	boxA.Build(a.v2);
	boxA.Build(a.v3);
	boxA.Check();
	boxB.Build(b.v1);
	boxB.Build(b.v2);
	boxB.Build(b.v3);
	boxB.Check();

	if (boxA.min.z > boxB.min.z)
		return true;
	else
		return false;
}

BVHNode* BVHNode::Construct(std::vector<Triangle>& triangles, int size, int offset)
{
	std::random_device rd;
	int axis = mRandAxis(std::mt19937(rd()));
	if (axis == 0)
		std::sort(triangles.begin() + offset, triangles.begin() + offset + size, TriXCompare);
	else if (axis == 1)
		std::sort(triangles.begin() + offset, triangles.begin() + offset + size, TriYCompare);
	else
		std::sort(triangles.begin() + offset, triangles.begin() + offset + size, TriZCompare);

	if (size == 0)
		return this;
	else if (size == 1)
	{
		mLeft = new BVHNode(&triangles[offset]);
		mRight = new BVHNode(&triangles[offset]);
	}
	else if (size == 2)
	{
		mLeft = new BVHNode(&triangles[offset]);
		mRight = new BVHNode(&triangles[offset + 1]);
	}
	else
	{
		int leftOffset = offset;
		int leftSize = size / 2;
		int rightOffset = offset + leftSize;
		int rightSize = size - leftSize;
		BVHNode* nodeLeft = new BVHNode();
		BVHNode* nodeRight = new BVHNode();
		mLeft = nodeLeft->Construct(triangles, leftSize, leftOffset);
		mRight = nodeRight->Construct(triangles, rightSize, rightOffset);
	}

	mBox.Build(mLeft->mBox.min);
	mBox.Build(mLeft->mBox.max);
	mBox.Build(mRight->mBox.min);
	mBox.Build(mRight->mBox.max);
	mBox.Check();

	return this;
}
