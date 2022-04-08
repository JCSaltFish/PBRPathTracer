#define _USE_MATH_DEFINES
#include <math.h>
#include <omp.h>

#include "pathtracer.h"
#include "OBJ_Loader.h"

PathTracer::PathTracer()
{
	mOutImg = 0;
	mTotalImg = 0;
	mMaxDepth = 3;

	mCamDir = glm::vec3(0.0f, 0.0f, 1.0f);
	mCamUp = glm::vec3(0.0f, 1.0f, 0.0f);
	mCamFocal = 0.1f;
	mCamFovy = 90;

	mSamples = 0;
}

PathTracer::~PathTracer()
{
	if (mTotalImg)
		delete[] mTotalImg;

	if (mBvh)
		delete mBvh;
}

void PathTracer::LoadMesh(std::string file, glm::mat4 model, Material material, bool ccw)
{
	int id = -1;

	objl::Loader loader;
	if (loader.LoadFile(file))
	{
		for (int i = 0; i < loader.LoadedIndices.size(); i += 3)
		{
			Triangle t;
			int index = loader.LoadedIndices[i];
			t.v1 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v1 = glm::vec3(model * glm::vec4(t.v1, 1.0f));

			index = loader.LoadedIndices[i + 1];
			t.v2 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v2 = glm::vec3(model * glm::vec4(t.v2, 1.0f));

			index = loader.LoadedIndices[i + 2];
			t.v3 = glm::vec3(loader.LoadedVertices[index].Position.X,
				loader.LoadedVertices[index].Position.Y,
				loader.LoadedVertices[index].Position.Z);
			t.v3 = glm::vec3(model * glm::vec4(t.v3, 1.0f));

			// normal
			glm::vec3 e1 = glm::normalize(t.v2 - t.v1);
			glm::vec3 e2 = glm::normalize(t.v3 - t.v1);
			t.normal = glm::normalize(glm::cross(e1, e2));
			if (ccw)
				t.normal = -t.normal;

			t.material = material;
			mTriangles.push_back(t);
		}

		// Build BVH
		if (mBvh)
			delete mBvh;
		mBvh = new BVHNode();
		mBvh->Construct(mTriangles);
	}
}

void PathTracer::SetOutImage(GLubyte* out)
{
	mOutImg = out;
}

void PathTracer::SetResolution(glm::ivec2 res)
{
	mResolution = res;

	mTotalImg = new float[res.x * res.y * 3];
}

glm::ivec2 PathTracer::GetResolution()
{
	return mResolution;
}

void PathTracer::SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up)
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

int PathTracer::GetSamples()
{
	return mSamples;
}

float PathTracer::Rand(glm::vec2 co, float& seed)
{
	seed++;
	return glm::fract(sinf(seed / mSamples * glm::dot(co, glm::vec2(12.9898f, 78.233f))) * 43758.5453f);
}

glm::vec3 PathTracer::Trace(glm::vec3 ro, glm::vec3 rd, glm::vec2 raySeed, float& randSeed, int depth)
{
	float d = 0.0f;
	Triangle t;
	if (mBvh->Hit(ro, rd, t, d))
	{
		Material mat = t.material;
		glm::vec3 n = t.normal;
		glm::vec3 p = ro + rd * d + n * EPS;
		if (depth < mMaxDepth * 2)
		{
			depth++;
			// Russian Roulette Path Termination
			float prob = glm::min(0.95f, glm::max(glm::max(mat.baseColor.x, mat.baseColor.y), mat.baseColor.z));
			if (depth >= mMaxDepth)
			{
				if (glm::abs(Rand(raySeed, randSeed)) > prob)
					return mat.emissive;
			}

			glm::vec3 r = glm::reflect(rd, n);
			glm::vec3 reflectDir;
			if (mat.type == MaterialType::SPECULAR)
				reflectDir = r;
			else if (mat.type == MaterialType::DIFFUSE)
			{
				// Monte Carlo Integration
				glm::vec3 u = glm::abs(n.x) < 1.0f - EPS ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
				u = glm::normalize(u);
				glm::vec3 v = glm::normalize(glm::cross(u, n));
				float w = Rand(raySeed, randSeed), theta = Rand(raySeed, randSeed);
				// uniformly sampling on hemisphere
				reflectDir = w * cosf(2.0f * M_PI * theta) * u + w * sinf(2.0f * M_PI * theta) * v + glm::sqrt(1.0f - w * w) * n;
				reflectDir = glm::normalize(reflectDir);
			}
			else if (mat.type == MaterialType::GLOSSY)
			{
				// Monte Carlo Integration
				glm::vec3 u = glm::abs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
				u = glm::normalize(u);
				glm::vec3 v = glm::cross(u, r);
				float w = Rand(raySeed, randSeed) * mat.roughness, theta = Rand(raySeed, randSeed);
				// wighted sampling on hemisphere
				reflectDir = w * cosf(2 * M_PI * theta) * u + w * sinf(2 * M_PI * theta) * v + glm::sqrt(1 - w * w) * r;
			}
			else if (mat.type == MaterialType::GLASS)
			{
				// TODO: Translusency here
				reflectDir = r;
			}

			return mat.emissive + Trace(p, reflectDir, raySeed, randSeed, depth) * mat.baseColor;
		}
	}

	return glm::vec3(0.0f);
}

void PathTracer::RenderFrame()
{
	mSamples++;
	//samples = 20;

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
		glm::vec3 pixel = topLeft - mCamUp * ((float)i * deltaY);
		for (int j = 0; j < mResolution.x; j++)
		{
			glm::vec3 rayDir = glm::normalize(pixel - mCamPos);
			float seed = 0.0f;
			glm::vec3 color = Trace(mCamPos, rayDir, glm::vec2(i, j), seed);

			// Draw
			int imgPixel = ((mResolution.y - 1 - i) * mResolution.x + j) * 3;

			mTotalImg[imgPixel] += color.r;
			mTotalImg[imgPixel + 1] += color.g;
			mTotalImg[imgPixel + 2] += color.b;

			glm::vec3 res = glm::vec3(mTotalImg[imgPixel] / mSamples,
				mTotalImg[imgPixel + 1] / mSamples,
				mTotalImg[imgPixel + 2] / mSamples);
			res = glm::clamp(res, glm::vec3(0.0f), glm::vec3(1.0f));

			mOutImg[imgPixel] = res.r * 255;
			mOutImg[imgPixel + 1] = res.g * 255;
			mOutImg[imgPixel + 2] = res.b * 255;

			pixel += camRight * deltaX;
		}
	}
}
