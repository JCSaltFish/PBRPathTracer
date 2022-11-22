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

	mRandAxis = std::uniform_int_distribution<int>(0, 2);
}

BVHNode::BVHNode(const Triangle& t) : mTriangle(t)
{
	mBox.Build(t.v1);
	mBox.Build(t.v2);
	mBox.Build(t.v3);
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

BVHNode* BVHNode::Construct(std::vector<Triangle>& triangles)
{
	std::random_device rd;
	int axis = mRandAxis(std::mt19937(rd()));
	if (axis == 0)
		std::sort(triangles.begin(), triangles.end(), TriXCompare);
	else if (axis == 1)
		std::sort(triangles.begin(), triangles.end(), TriYCompare);
	else
		std::sort(triangles.begin(), triangles.end(), TriZCompare);

	if (triangles.size() == 0)
		return this;
	else if (triangles.size() == 1)
	{
		mLeft = new BVHNode(triangles[0]);
		mRight = new BVHNode(triangles[0]);
	}
	else if (triangles.size() == 2)
	{
		mLeft = new BVHNode(triangles[0]);
		mRight = new BVHNode(triangles[1]);
	}
	else
	{
		std::vector<Triangle> left, right;
		int i = 0;
		for (; i < triangles.size() / 2; i++)
			left.push_back(triangles[i]);
		for (; i < triangles.size(); i++)
			right.push_back(triangles[i]);
		BVHNode* nodeLeft = new BVHNode();
		BVHNode* nodeRight = new BVHNode();
		mLeft = nodeLeft->Construct(left);
		mRight = nodeRight->Construct(right);
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

const bool BVHNode::Hit(const glm::vec3& ro, const glm::vec3& rd, Triangle& triangleOut, float& distOut)
{
	if (mLeft && mRight)
	{
		if (mBox.Intersect(ro, rd))
		{
			Triangle tLeft, tRight;
			float dLeft, dRight;
			bool hitLeft = mLeft->Hit(ro, rd, tLeft, dLeft);
			bool hitRight = mRight->Hit(ro, rd, tRight, dRight);
			if (hitLeft && hitRight)
			{
				if (dLeft < dRight)
				{
					triangleOut = tLeft;
					distOut = dLeft;
				}
				else
				{
					triangleOut = tRight;
					distOut = dRight;
				}
				return true;
			}
			else if (hitLeft)
			{
				triangleOut = tLeft;
				distOut = dLeft;
				return true;
			}
			else if (hitRight)
			{
				triangleOut = tRight;
				distOut = dRight;
				return true;
			}
			else
				return false;
		}
		else
			return false;
	}
	else
	{
		if (glm::dot(rd, mTriangle.normal) == 0.0f)
			return false;
		distOut = glm::dot((mTriangle.v1 - ro), mTriangle.normal) / glm::dot(rd, mTriangle.normal);
		if (distOut < 0)
			return false;
		// update intersection point
		glm::vec3 p = ro + rd * distOut;

        if (IsInside(p, mTriangle.v1, mTriangle.v2, mTriangle.v3)) {
            triangleOut = mTriangle;
            return true;
        }
        return false;
	}

	return false;
}

void BVHNode::GetGPULayout(std::vector<GPUBVHNode>& bvh)
{
	GPUBVHNode node;
	node.box = mBox;
	node.triangle = mTriangle;

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
