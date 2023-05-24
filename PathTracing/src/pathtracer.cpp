#define _USE_MATH_DEFINES
#include <sstream>
#include <math.h>

#include <omp.h>

#include <tiny_obj_loader.h>

#include "pathtracer.h"

PathTracer::PathTracer() : mRng(std::random_device()())
{
	mOutImg = 0;
	mTotalImg = 0;
	mMaxDepth = 3;

	mCamDir = glm::vec3(0.0f, 0.0f, 1.0f);
	mCamUp = glm::vec3(0.0f, 1.0f, 0.0f);
	mCamFocal = 0.1f;
	mCamFovy = 90;
	mCamFocalDist = 5.0f;
	mCamAperture = 0.0f;

	mSamples = 0;
	mNeedReset = false;
	mExit = false;
}

PathTracer::~PathTracer()
{
	if (mTotalImg)
		delete[] mTotalImg;

	if (mBvh)
		delete mBvh;

	for (auto texture : mLoadedTextures)
		delete texture;
}

void PathTracer::LoadObject(const std::string& file, const glm::mat4& model)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	if (tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file.c_str()))
	{
		int nameStartIndex = file.find_last_of('/') + 1;
		if (nameStartIndex > file.size() - 1)
			nameStartIndex = 0;
		int nameEndIndex = file.find_last_of(".");
		if (nameEndIndex == std::string::npos)
			nameEndIndex = file.size() - 1;
		std::string objName = file.substr(nameStartIndex, nameEndIndex - nameStartIndex);
		PathTracerLoader::Object obj(objName);

		for (int i = 0; i < shapes.size(); i++)
		{
			std::string elementName = shapes[i].name;
			PathTracerLoader::Element element(elementName);
			obj.elements.push_back(element);

			for (int j = 0; j < shapes[i].mesh.num_face_vertices.size(); j++)
			{
				if (shapes[i].mesh.num_face_vertices[j] != 3)
					continue;

				Triangle t;

				int vi = shapes[i].mesh.indices[j * 3].vertex_index;
				int ti = shapes[i].mesh.indices[j * 3].texcoord_index;
				int ni = shapes[i].mesh.indices[j * 3].normal_index;
				t.v1 = glm::vec3(-attrib.vertices[3 * vi],
					attrib.vertices[3 * vi + 1],
					attrib.vertices[3 * vi + 2]);
				t.v1 = glm::vec3(model * glm::vec4(t.v1, 1.0f));
				if (attrib.normals.size() != 0)
				{
					t.n1 = glm::vec3(-attrib.normals[3 * ni],
						attrib.normals[3 * ni + 1],
						attrib.normals[3 * ni + 2]);
					t.n1 = glm::vec3(model * glm::vec4(t.n1, 0.0f));
				}
				if (attrib.texcoords.size() != 0)
				{
					t.uv1 = glm::vec2(attrib.texcoords[2 * ti],
						1.0f - attrib.texcoords[2 * ti + 1]);
				}

				vi = shapes[i].mesh.indices[j * 3 + 1].vertex_index;
				ti = shapes[i].mesh.indices[j * 3 + 1].texcoord_index;
				ni = shapes[i].mesh.indices[j * 3 + 1].normal_index;
				t.v2 = glm::vec3(-attrib.vertices[3 * vi],
					attrib.vertices[3 * vi + 1],
					attrib.vertices[3 * vi + 2]);
				t.v2 = glm::vec3(model * glm::vec4(t.v2, 1.0f));
				if (attrib.normals.size() != 0)
				{
					t.n2 = glm::vec3(-attrib.normals[3 * ni],
						attrib.normals[3 * ni + 1],
						attrib.normals[3 * ni + 2]);
					t.n2 = glm::vec3(model * glm::vec4(t.n2, 0.0f));
				}
				if (attrib.texcoords.size() != 0)
					t.uv2 = glm::vec2(attrib.texcoords[2 * ti],
						1.0f - attrib.texcoords[2 * ti + 1]);

				vi = shapes[i].mesh.indices[j * 3 + 2].vertex_index;
				ti = shapes[i].mesh.indices[j * 3 + 2].texcoord_index;
				ni = shapes[i].mesh.indices[j * 3 + 2].normal_index;
				t.v3 = glm::vec3(-attrib.vertices[3 * vi],
					attrib.vertices[3 * vi + 1],
					attrib.vertices[3 * vi + 2]);
				t.v3 = glm::vec3(model * glm::vec4(t.v3, 1.0f));
				if (attrib.normals.size() != 0)
				{
					t.n3 = glm::vec3(-attrib.normals[3 * ni],
						attrib.normals[3 * ni + 1],
						attrib.normals[3 * ni + 2]);
					t.n3 = glm::vec3(model * glm::vec4(t.n3, 0.0f));
				}
				if (attrib.texcoords.size() != 0)
				{
					t.uv3 = glm::vec2(attrib.texcoords[2 * ti],
						1.0f - attrib.texcoords[2 * ti + 1]);
				}

				t.Init();

				if (shapes[i].mesh.smoothing_group_ids.size() != 0)
				{
					if (shapes[i].mesh.smoothing_group_ids[j] != 0)
						t.smoothing = true;
				}

				t.objectId = mLoadedObjects.size();
				t.elementId = i;

				mTriangles.push_back(t);
			}
		}
		mLoadedObjects.push_back(obj);
	}
}

void PathTracer::SetDiffuseTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.diffuseTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.diffuseTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.diffuseTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetNormalTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.normalTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.normalTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.normalTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetEmissTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.emissTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.emissTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.emissTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetRoughnessTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.roughnessTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.roughnessTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.roughnessTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetMetallicTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.metallicTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.metallicTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.metallicTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetOpacityTextureForElement(int objId, int elementId, const std::string& file)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.opacityTex)
	{
		auto it = std::find(mLoadedTextures.begin(), mLoadedTextures.end(), mat.opacityTex);
		(*it)->Load(file);
	}
	else
	{
		Image* texture = new Image(file);
		mat.opacityTex = texture;
		mLoadedTextures.push_back(texture);
	}
}

void PathTracer::SetMaterial(int objId, int elementId, Material& material)
{
	if (objId >= mLoadedObjects.size())
		return;
	if (elementId >= mLoadedObjects[objId].elements.size())
		return;

	material.diffuseTex = mLoadedObjects[objId].elements[elementId].material.diffuseTex;
	material.normalTex = mLoadedObjects[objId].elements[elementId].material.normalTex;
	material.emissTex = mLoadedObjects[objId].elements[elementId].material.emissTex;
	material.roughnessTex = mLoadedObjects[objId].elements[elementId].material.roughnessTex;
	material.metallicTex = mLoadedObjects[objId].elements[elementId].material.metallicTex;
	material.opacityTex = mLoadedObjects[objId].elements[elementId].material.opacityTex;

	mLoadedObjects[objId].elements[elementId].material = material;
}

void PathTracer::BuildBVH()
{
	if (mBvh)
		delete mBvh;
	mBvh = new BVHNode();
	mBvh->Construct(mTriangles, mTriangles.size());

	std::vector<Triangle*>().swap(mLights);
	for (auto& t : mTriangles)
	{
		t.mat = &mLoadedObjects[t.objectId].elements[t.elementId].material;
		if (glm::length(t.mat->emissive) >= EPS)
			mLights.push_back(&t);
	}
}

void PathTracer::ResetImage()
{
	mNeedReset = true;
}

void PathTracer::ClearScene()
{
	mTriangles.swap(std::vector<Triangle>());
	mLoadedObjects.swap(std::vector<PathTracerLoader::Object>());
	if (mBvh)
		delete mBvh;
	mBvh = 0;
	for (auto texture : mLoadedTextures)
		delete texture;
	mLoadedTextures.swap(std::vector<Image*>());

	if (mTotalImg)
		delete[] mTotalImg;
	mTotalImg = 0;
}

void PathTracer::SetOutImage(GLubyte* out)
{
	mOutImg = out;
}

void PathTracer::SetResolution(const glm::ivec2& res)
{
	mResolution = res;
	mTotalImg = new float[res.x * res.y * 3];
}

std::vector<PathTracerLoader::Object> PathTracer::GetLoadedObjects() const
{
	return mLoadedObjects;
}

const glm::ivec2 PathTracer::GetResolution() const
{
	return mResolution;
}

const int PathTracer::GetTriangleCount() const
{
	return mTriangles.size();
}

const int PathTracer::GetTraceDepth() const
{
	return mMaxDepth;
}

void PathTracer::SetTraceDepth(int depth)
{
	mMaxDepth = depth;
}

void PathTracer::SetCamera(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up)
{
	mCamPos = pos;
	mCamDir = glm::normalize(dir);
	mCamUp = glm::normalize(up);
}

void PathTracer::SetProjection(float f, float fovy)
{
	mCamFocal = f;
	if (mCamFocal <= 0.0f)
		mCamFocal = 0.1f;
	mCamFovy = fovy;
	if (mCamFovy <= 0.0f)
		mCamFovy = 0.1f;
	else if (mCamFovy >= 180.0f)
		mCamFovy = 179.5;
}

void PathTracer::SetCameraFocalDist(float dist)
{
	mCamFocalDist = dist;
}

void PathTracer::SetCameraAperture(float aperture)
{
	mCamAperture = aperture;
}

const int PathTracer::GetSamples() const
{
	return mSamples;
}

const float PathTracer::Rand()
{
	std::uniform_real_distribution<float> dis(0.0f, 1.0f);
	return dis(mRng);
}

const bool PathTracer::IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b) const
{
	glm::vec3 ba = b - a;
	glm::vec3 cp1 = glm::cross(ba, (p1 - a));
	glm::vec3 cp2 = glm::cross(ba, (p2 - a));

	return (glm::dot(cp1, cp2) >= 0);
}

const bool PathTracer::IntersectTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) const
{
	return (IsSameSide(p, a, b, c) && IsSameSide(p, b, a, c) && IsSameSide(p, c, a, b));
}

const bool PathTracer::Hit(BVHNode* node, const glm::vec3& ro, const glm::vec3& rd, Triangle*& triangleOut, float& distOut)
{
	if (node->mLeft && node->mRight)
	{
		if (node->mBox.Intersect(ro, rd))
		{
			Triangle* tLeft = 0;
			Triangle* tRight = 0;
			float dLeft, dRight;
			bool hitLeft = Hit(node->mLeft, ro, rd, tLeft, dLeft);
			bool hitRight = Hit(node->mRight, ro, rd, tRight, dRight);
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
	else if (node->mTriangle)
	{
		if (glm::dot(rd, node->mTriangle->normal) == 0.0f)
			return false;
		distOut = glm::dot((node->mTriangle->v1 - ro), node->mTriangle->normal) / glm::dot(rd, node->mTriangle->normal);
		if (distOut < 0)
			return false;
		// update intersection point
		glm::vec3 p = ro + rd * distOut;

		if (IntersectTriangle(p, node->mTriangle->v1, node->mTriangle->v2, node->mTriangle->v3))
		{
			bool result = true;
			// Opacity
			if (node->mTriangle->mat->opacityTex)
			{
				glm::vec2 uv = GetUV(p, node->mTriangle);
				float opacity = node->mTriangle->mat->opacityTex->tex2D(uv).r;
				result = Rand() < opacity;
			}

			if (result)
			{
				triangleOut = node->mTriangle;
				return true;
			}
			else
				return false;
		}
		return false;
	}

	return false;
}

const glm::vec3 PathTracer::SampleTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
	float u = sqrt(Rand());
	float v = Rand();
	float w0 = 1.f - u;
	float w1 = u * (1.f - v);
	float w2 = u * v;
	return w0 * v0 + w1 * v1 + w2 * v2;
}

const glm::vec3 PathTracer::DirectIllumimation(const glm::vec3& rd, const glm::vec3& p, const glm::vec3& n, const glm::vec3& diffuse)
{
	// sample a light triangle
	int lightId = int(floor(Rand() * mLights.size()));
	if (lightId == mLights.size() && lightId > 0)
		lightId--;
	Triangle* tLight = mLights[lightId];
	// sample a point inside the triangle
	glm::vec3 vLight = SampleTriangle(tLight->v1, tLight->v2, tLight->v3);
	// evaluate the shadow ray
	glm::vec3 l = glm::normalize(vLight - p);
	float d = 0.0;
	Triangle* t = 0;
	if (Hit(mBvh, p, l, t, d))
	{
		if (tLight != t)
			return glm::vec3(0.f);
	}

	glm::vec3 lColor = tLight->mat->emissive * tLight->mat->emissiveIntensity;

	return lColor * diffuse * glm::max(0.f, glm::dot(-n, -l));
}

const glm::vec2 PathTracer::GetUV(const glm::vec3& p, Triangle* t) const
{
	glm::vec3 v2 = p - t->v1;
	float d20 = glm::dot(v2, t->barycentricInfo.v0);
	float d21 = glm::dot(v2, t->barycentricInfo.v1);
	
	float alpha = (t->barycentricInfo.d11 * d20 - t->barycentricInfo.d01 * d21) *
		t->barycentricInfo.invDenom;
	float beta = (t->barycentricInfo.d00 * d21 - t->barycentricInfo.d01 * d20) *
		t->barycentricInfo.invDenom;

	return (1.0f - alpha - beta) * t->uv1 + alpha * t->uv2 + beta * t->uv3;
}

const glm::vec3 PathTracer::GetSmoothNormal(const glm::vec3& p, Triangle* t) const
{
	glm::vec3 v2 = p - t->v1;
	float d20 = glm::dot(v2, t->barycentricInfo.v0);
	float d21 = glm::dot(v2, t->barycentricInfo.v1);

	float alpha = (t->barycentricInfo.d11 * d20 - t->barycentricInfo.d01 * d21) *
		t->barycentricInfo.invDenom;
	float beta = (t->barycentricInfo.d00 * d21 - t->barycentricInfo.d01 * d20) *
		t->barycentricInfo.invDenom;

	glm::vec3 n = (1.0f - alpha - beta) * t->n1 + alpha * t->n2 + beta * t->n3;
	glm::vec3 res = glm::normalize(glm::vec3(n.x, -n.y, n.z));
	return glm::normalize(n);
}

const glm::vec3 PathTracer::Trace(const glm::vec3& ro, const glm::vec3& rd, int depth, int iter, bool inside)
{
	float d = 0.0f;
	Triangle* t = 0;
	if (Hit(mBvh, ro, rd, t, d))
	{
		Material& mat = mLoadedObjects[t->objectId].elements[t->elementId].material;
		glm::vec3 p = ro + rd * d;
		glm::vec2 uv = GetUV(p, t);
		glm::vec3 n = t->normal;
		if (t->smoothing)
			n = GetSmoothNormal(p, t);
		if (mat.normalTex)
		{
			glm::mat3 TBN = glm::mat3(t->tangent, t->bitangent, n);
			glm::vec3 nt = glm::vec3(mat.normalTex->tex2D(uv)) * 2.0f - 1.0f;
			if (nt.z <= 0.0f)
				nt = glm::vec3(nt.x, nt.y, EPS);
			nt = glm::normalize(nt);
			n = glm::normalize(TBN * nt);
		}
		if (glm::dot(n, rd) > 0.0f)
			n = -n;
		p += n * EPS;

		if (iter < mMaxDepth)
		{
			glm::vec3 diffuse = mat.diffuse;
			if (mat.diffuseTex)
				diffuse = glm::vec3(mat.diffuseTex->tex2D(uv));
			glm::vec3 emiss = mat.emissive;
			if (mat.emissTex)
				emiss = glm::vec3(mat.emissTex->tex2D(uv));
			float roughness = mat.roughness;
			if (mat.roughnessTex)
				roughness = mat.roughnessTex->tex2D(uv).r;
			float reflectiveness = mat.reflectiveness;
			if (mat.metallicTex)
				reflectiveness = mat.metallicTex->tex2D(uv).r;

			depth++;
			iter++;
			// Russian Roulette Path Termination
			float prob = glm::min(0.95f, glm::max(glm::max(mat.diffuse.x, mat.diffuse.y), mat.diffuse.z));
			if (depth >= mMaxDepth)
			{
				if (glm::abs(Rand()) > prob)
					return glm::vec3(0.0f);
			}

			glm::vec3 r = glm::reflect(rd, n);
			glm::vec3 reflectDir;
			
			if (mat.type == MaterialType::OPAQUE)
			{
				if (Rand() < reflectiveness)
				{
					if (roughness == 1.0f)
					{
						// uniformly sampling on hemisphere
						glm::vec3 u = fabs(n.x) < 1.0f - EPS ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
						u = glm::normalize(u);
						glm::vec3 v = glm::normalize(glm::cross(u, n));
						float w = Rand(), theta = Rand();
						reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * n;
						reflectDir = glm::normalize(reflectDir);
					}
					else if (roughness == 0.0f)
						reflectDir = r;
					else
					{
						// wighted sampling on hemisphere
						glm::vec3 u = fabs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
						u = glm::normalize(u);
						glm::vec3 v = glm::normalize(glm::cross(u, r));
						float w = Rand() * roughness, theta = Rand();
						reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * r;
						reflectDir = glm::normalize(reflectDir);
					}
					iter--;
					return emiss * mat.emissiveIntensity + Trace(p, reflectDir, depth, iter, inside) * mat.specular;
				}
				else
				{
					// uniformly sampling on hemisphere
					glm::vec3 u = fabs(n.x) < 1.0f - EPS ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
					u = glm::normalize(u);
					glm::vec3 v = glm::normalize(glm::cross(u, n));
					float w = Rand(), theta = Rand();
					reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * n;
					reflectDir = glm::normalize(reflectDir);

					return emiss * mat.emissiveIntensity + DirectIllumimation(rd, p, n, diffuse) + Trace(p, reflectDir, depth, iter, inside) * diffuse;
				}
			}
			else
			{
				bool refract = false;
				glm::vec3 refractN = n;
				if (roughness != 0.0f)
				{
					// wighted sampling on hemisphere
					glm::vec3 u = fabs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
					u = glm::normalize(u);
					glm::vec3 v = glm::normalize(glm::cross(u, r));
					float w = Rand() * roughness, theta = Rand();
					refractN = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * n;
					refractN = glm::normalize(refractN);
				}

				float nc = 1.0f, ng = mat.ior;
				// Snells law
				float eta = inside ? ng / nc : nc / ng;
				float r0 = (nc - ng) / (nc + ng);
				r0 = r0 * r0;
				float c = fabs(glm::dot(rd, refractN));
				float k = 1.0f - eta * eta * (1.0f - c * c);
				if (k < 0.0f)
					refract = false;
				else
				{
					// Shilick's approximation of Fresnel's equation
					float re = r0 + (1.0f - r0) * (1.0f - c) * (1.0f - c);
					if (fabs(Rand()) < re)
						refract = false;
					else if (Rand() < reflectiveness)
						refract = false;
					else
						refract = true;
				}

				iter--;
				if (!refract)
				{
					if (roughness == 1.0f)
					{
						// uniformly sampling on hemisphere
						glm::vec3 u = fabs(n.x) < 1.0f - EPS ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
						u = glm::normalize(u);
						glm::vec3 v = glm::normalize(glm::cross(u, n));
						float w = Rand(), theta = Rand();
						reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * n;
						reflectDir = glm::normalize(reflectDir);
					}
					else if (roughness == 0.0f)
						reflectDir = r;
					else
					{
						// wighted sampling on hemisphere
						glm::vec3 u = fabs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
						u = glm::normalize(u);
						glm::vec3 v = glm::normalize(glm::cross(u, r));
						float w = Rand() * roughness, theta = Rand();
						reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * r;
						reflectDir = glm::normalize(reflectDir);
					}
					return emiss * mat.emissiveIntensity + Trace(p, reflectDir, depth, iter, inside) * mat.specular;
				}
				else
				{
					if (Rand() < mat.translucency)
					{
						reflectDir = glm::normalize(eta * rd - (eta * glm::dot(n, rd) + sqrtf(k)) * refractN);
						p -= n * EPS * 2.0f;
						inside = !inside;
						return emiss * mat.emissiveIntensity + Trace(p, reflectDir, depth, iter, inside) * diffuse;
					}
					else
					{
						// uniformly sampling on hemisphere
						glm::vec3 u = fabs(n.x) < 1.0f - EPS ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
						u = glm::normalize(u);
						glm::vec3 v = glm::normalize(glm::cross(u, n));
						float w = Rand(), theta = Rand();
						reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + sqrtf(1.0f - w * w) * n;
						reflectDir = glm::normalize(reflectDir);

						return emiss * mat.emissiveIntensity + DirectIllumimation(rd, p, n, diffuse) + Trace(p, reflectDir, depth, iter, inside) * diffuse;
					}
				}
			}
		}
	}

	return glm::vec3(0.0f);
}

const glm::vec2 PathTracer::SampleCircle()
{
	float angle = Rand() * 2. * M_PI;
	float radius = sqrt(Rand());
	return glm::vec2(cos(angle), sin(angle)) * radius;
}

void PathTracer::RenderFrame()
{
	mExit = false;

	if (mNeedReset)
	{
		for (int i = 0; i < mResolution.x * mResolution.y * 3; i++)
			mTotalImg[i] = 0.0f;
		mNeedReset = false;
        mSamples = 0;
	}

	mSamples++;

	// Position world space image plane
	glm::vec3 imgCenter = mCamPos + mCamDir * mCamFocal;
	float imgHeight = 2.0f * mCamFocal * tan((mCamFovy / 2.0f) * M_PI / 180.0f);
	float aspect = (float)mResolution.x / (float)mResolution.y;
	float imgWidth = imgHeight * aspect;
	float deltaX = imgWidth / (float)mResolution.x;
	float deltaY = imgHeight / (float)mResolution.y;
	glm::vec3 camRight = glm::normalize(glm::cross(mCamUp, mCamDir));

	// Starting at top left
	glm::vec3 topLeft = imgCenter - camRight * (imgWidth * 0.5f);
	topLeft += mCamUp * (imgHeight * 0.5f);

	int numThreads = omp_get_max_threads();
	if (numThreads > 2)
		numThreads -= 3;
	else if (numThreads > 1)
		numThreads -= 2;
	else if (numThreads > 0)
		numThreads--;
	// Loop through each pixel
	#pragma omp parallel for num_threads(numThreads)
	for (int i = 0; i < mResolution.y; i++)
	{
		if (mExit)
			break;

		glm::vec3 pixel = topLeft - mCamUp * ((float)i * deltaY);
		for (int j = 0; j < mResolution.x; j++)
		{
			glm::vec3 rayDir = glm::normalize(pixel - mCamPos);
			// DOF
			glm::vec3 camPos = mCamPos;
			glm::vec3 focalPoint = camPos + rayDir * mCamFocalDist;
			glm::vec2 camPosOffset = SampleCircle() * mCamAperture;
			camPos += camRight * camPosOffset.x + mCamUp * camPosOffset.y;
			rayDir = glm::normalize(focalPoint - camPos);

			glm::vec3 color = Trace(camPos, rayDir);

			// Draw
			int imgPixel = ((mResolution.y - 1 - i) * mResolution.x + j) * 3;

			mTotalImg[imgPixel] += color.r;
			mTotalImg[imgPixel + 1] += color.g;
			mTotalImg[imgPixel + 2] += color.b;

			glm::vec3 res = glm::vec3
			(
				mTotalImg[imgPixel] / (float)mSamples,
				mTotalImg[imgPixel + 1] / (float)mSamples,
				mTotalImg[imgPixel + 2] / (float)mSamples
			);
			res = glm::clamp(res, glm::vec3(0.0f), glm::vec3(1.0f));

			mOutImg[imgPixel] = res.r * 255;
			mOutImg[imgPixel + 1] = res.g * 255;
			mOutImg[imgPixel + 2] = res.b * 255;

			pixel += camRight * deltaX;
		}
	}
}

void PathTracer::Exit()
{
	mExit = true;
}
