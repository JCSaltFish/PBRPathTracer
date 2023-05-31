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
	// TBN
	glm::vec3 e1 = v2 - v1;
	glm::vec3 e2 = v3 - v1;
	glm::vec2 deltaUv1 = uv2 - uv1;
	glm::vec2 deltaUv2 = uv3 - uv1;
	float f = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y);

	float nx = e1.y * e2.z - e1.z * e2.y;
	float ny = e1.z * e2.x - e1.x * e2.z;
	float nz = e1.x * e2.y - e1.y * e2.x;
	normal = glm::normalize(glm::vec3(nx, ny, nz));

	float tx = f * (deltaUv2.y * e1.x - deltaUv1.y * e2.x);
	float ty = f * (deltaUv2.y * e1.y - deltaUv1.y * e2.y);
	float tz = f * (deltaUv2.y * e1.z - deltaUv1.y * e2.z);
	tangent = glm::normalize(glm::vec3(tx, ty, tz));

	bitangent = glm::normalize(glm::cross(normal, tangent));
}

BVHNode::BVHNode()
{
	mTriangleId = -1;

	mLeft = 0;
	mRight = 0;

	mRandAxis = std::uniform_int_distribution<int>(0, 2);
}

BVHNode::BVHNode(const GPUTriangle& t)
{
	mBox.Build(glm::vec3(t.v1));
	mBox.Build(glm::vec3(t.v2));
	mBox.Build(glm::vec3(t.v3));
	mBox.Check();

	mTriangleId = t.id;

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

bool BVHNode::TriXCompare(const GPUTriangle& a, const GPUTriangle& b)
{
	AABB boxA, boxB;
	boxA.Build(glm::vec3(a.v1));
	boxA.Build(glm::vec3(a.v2));
	boxA.Build(glm::vec3(a.v3));
	boxA.Check();
	boxB.Build(glm::vec3(b.v1));
	boxB.Build(glm::vec3(b.v2));
	boxB.Build(glm::vec3(b.v3));
	boxB.Check();

	if (boxA.min.x > boxB.min.x)
		return true;
	else
		return false;
}

bool BVHNode::TriYCompare(const GPUTriangle& a, const GPUTriangle& b)
{
	AABB boxA, boxB;
	boxA.Build(glm::vec3(a.v1));
	boxA.Build(glm::vec3(a.v2));
	boxA.Build(glm::vec3(a.v3));
	boxA.Check();
	boxB.Build(glm::vec3(b.v1));
	boxB.Build(glm::vec3(b.v2));
	boxB.Build(glm::vec3(b.v3));
	boxB.Check();

	if (boxA.min.y > boxB.min.y)
		return true;
	else
		return false;
}

bool BVHNode::TriZCompare(const GPUTriangle& a, const GPUTriangle& b)
{
	AABB boxA, boxB;
	boxA.Build(glm::vec3(a.v1));
	boxA.Build(glm::vec3(a.v2));
	boxA.Build(glm::vec3(a.v3));
	boxA.Check();
	boxB.Build(glm::vec3(b.v1));
	boxB.Build(glm::vec3(b.v2));
	boxB.Build(glm::vec3(b.v3));
	boxB.Check();

	if (boxA.min.z > boxB.min.z)
		return true;
	else
		return false;
}

BVHNode* BVHNode::Construct(std::vector<GPUTriangle>& triangles, int size, int offset)
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
		mLeft = new BVHNode(triangles[offset]);
		mRight = new BVHNode(triangles[offset]);
	}
	else if (size == 2)
	{
		mLeft = new BVHNode(triangles[offset]);
		mRight = new BVHNode(triangles[offset + 1]);
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

// based from:
// https://blackpawn.com/texts/pointinpoly/?fbclid=IwAR2utQtHuFUrHXRszp5sP8CP3jJuMNsOVrpwqAWWSBIx6DLENK5T9lkMceA
const bool BVHNode::IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b)
{
	glm::vec3 ba = b - a;
	glm::vec3 cp1 = glm::cross(ba, (p1 - a));
	glm::vec3 cp2 = glm::cross(ba, (p2 - a));

	return (glm::dot(cp1, cp2) >= 0);
}

const bool BVHNode::IsInside(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	return (IsSameSide(p, a, b, c) && IsSameSide(p, b, a, c) && IsSameSide(p, c, a, b));
}

void BVHNode::GetGPULayout(std::vector<GPUBVHNode>& bvh)
{
	GPUBVHNode node;
	node.rightOffset = -1;
	node.aabb_min = glm::vec4(mBox.min, 1.f);
	node.aabb_max = glm::vec4(mBox.max, 1.f);
	node.triangleId = mTriangleId;

	int nodePos = bvh.size();
	node.nodeIndex = nodePos;
	bvh.push_back(node);
	int offset = -1;

	// offset for leaf node is -1
	if (!mLeft && !mRight)
		return;

	if (mLeft)
		mLeft->GetGPULayout(bvh);

	if (mRight)
	{
		offset = bvh.size() - nodePos;
		mRight->GetGPULayout(bvh);
	}
	else // left only
		offset = 0;

	bvh[nodePos].rightOffset = offset;
}
