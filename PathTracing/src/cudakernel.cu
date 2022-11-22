#include <glm/gtx/constants.hpp>

#include <curand_kernel.h>

#include "cudakernel.cuh"

__constant__ curandState_t* state = 0;

__constant__ float PI;
__constant__ float EPSILON = 0.001f;

__constant__ int resX, resY;
__constant__ int maxDepth;
__constant__ int samples;

__constant__ float _camPos[3], _camDir[3], _camUp[3];
__constant__ float camFocal, camFovy;

__device__ GPUBVHNode* bvh = 0;
__constant__ int bvhSize;

struct GPUImage
{
	int width;
	int height;
	float* data;
	__host__ __device__ GPUImage() :
		width(0),
		height(0),
		data(0)
	{
	}
};
__device__ GPUImage* textures = 0;
int numTextures = 0;

__global__ void InitCuRand(int seed)
{
	const int x = threadIdx.x + blockIdx.x * blockDim.x;
	const int y = threadIdx.y + blockIdx.y * blockDim.y;
	if (x >= resX || y >= resY)
		return;

	curand_init(seed, x + y * resX, 0, &state[x + y * resX]);
}

void InitCUDA()
{
	float h_pi = glm::pi<float>();
	gpuErrchk(cudaMemcpyToSymbol(PI, &h_pi, sizeof(float)));
	gpuErrchk(cudaDeviceSetLimit(cudaLimitStackSize, 1024 * 8));
}

void CUDASetResolution(int x, int y)
{
	gpuErrchk(cudaMemcpyToSymbol(resX, &x, sizeof(unsigned)));
	gpuErrchk(cudaMemcpyToSymbol(resY, &y, sizeof(unsigned)));

	if (state)
		gpuErrchk(cudaFree(state));
	curandState_t* d_randState;
	gpuErrchk(cudaMalloc((void**)&d_randState, x * y * sizeof(curandState_t)));
	gpuErrchk(cudaMemcpyToSymbol(state, &d_randState, sizeof(d_randState)));

	srand(time(0));
	int seed = rand();
	dim3 blockDim(16, 16, 1), gridDim(x / blockDim.x + 1, y / blockDim.y + 1, 1);
	InitCuRand << < gridDim, blockDim >> > (seed);
	gpuErrchk(cudaGetLastError());
	gpuErrchk(cudaDeviceSynchronize());
}

void CUDASetTraceDepth(int depth)
{
	gpuErrchk(cudaMemcpyToSymbol(maxDepth, &depth, sizeof(unsigned)));
}

void CUDASetCamera(float* pos, float* dir, float* up)
{
	gpuErrchk(cudaMemcpyToSymbol(_camPos, pos, sizeof(float) * 3));
	gpuErrchk(cudaMemcpyToSymbol(_camDir, dir, sizeof(float) * 3));
	gpuErrchk(cudaMemcpyToSymbol(_camUp, up, sizeof(float) * 3));
}

void CUDASetProjection(float f, float fovy)
{
	gpuErrchk(cudaMemcpyToSymbol(camFocal, &f, sizeof(float)));
	gpuErrchk(cudaMemcpyToSymbol(camFovy, &fovy, sizeof(float)));
}

void CUDASetBVH(GPUBVHNode* nodes, int size)
{
	gpuErrchk(cudaMemcpyToSymbol(bvhSize, &size, sizeof(unsigned)));

	BVHNode* d_Nodes;
	gpuErrchk(cudaMalloc((void**)&d_Nodes, size * sizeof(GPUBVHNode)));
	gpuErrchk(cudaMemcpy(d_Nodes, nodes, size * sizeof(GPUBVHNode), cudaMemcpyHostToDevice));
	gpuErrchk(cudaMemcpyToSymbol(bvh, &d_Nodes, sizeof(GPUBVHNode*)));
}

void CUDALoadTextures(const std::vector<Image*>& texVec)
{
	int size = texVec.size();
	numTextures = size;

	GPUImage* h_normalImgs = new GPUImage[size];
	for (int i = 0; i < size; i++)
	{
		int w = texVec[i]->width();
		int h = texVec[i]->height();
		h_normalImgs[i].width = w;
		h_normalImgs[i].height = h;

		int size = w * h * 4 * sizeof(float);
		float* h_data = new float[size];
		memcpy(h_data, texVec[i]->data(), size);
		float* d_data;
		gpuErrchk(cudaMalloc(&d_data, size));
		gpuErrchk(cudaMemcpy(d_data, h_data, size, cudaMemcpyHostToDevice));
		delete[] h_data;
		h_normalImgs->data = d_data;
	}

	GPUImage* d_normalImgs;
	gpuErrchk(cudaMalloc(&d_normalImgs, size * sizeof(GPUImage)));
	gpuErrchk(cudaMemcpy(d_normalImgs, h_normalImgs, size * sizeof(GPUImage), cudaMemcpyHostToDevice));
	gpuErrchk(cudaMemcpyToSymbol(textures, &d_normalImgs, size * sizeof(GPUImage*)));
	delete[] h_normalImgs;
}

__device__ glm::vec4 CUDATex2D(const GPUImage& image, const glm::vec2& uv)
{
	if (uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f)
		return glm::vec4(0.0f);

	int w = image.width;
	int h = image.height;

	glm::ivec2 coord = glm::ivec2(w * uv.x, h * uv.y);
	float* p = image.data + (4 * (coord.y * w + coord.x));

	return glm::vec4(p[0], p[1], p[2], p[3]);
}

__device__ bool IsSameSide(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& a, const glm::vec3& b)
{
	glm::vec3 ba = b - a;
	glm::vec3 cp1 = glm::cross(ba, (p1 - a));
	glm::vec3 cp2 = glm::cross(ba, (p2 - a));

	return (glm::dot(cp1, cp2) >= 0);
}

__device__ bool IsInside(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	return (IsSameSide(p, a, b, c) && IsSameSide(p, b, a, c) && IsSameSide(p, c, a, b));
}

__device__ bool IntersectBox(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& bMin, const glm::vec3& bMax)
{
	glm::vec3 tMin = (bMin - ro) / rd;
	glm::vec3 tMax = (bMax - ro) / rd;
	glm::vec3 t1 = glm::min(tMin, tMax);
	glm::vec3 t2 = glm::max(tMin, tMax);
	float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
	float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);
	if (tNear >= tFar)
		return false;
	return true;
}

__device__ bool Hit(const glm::vec3& ro, const glm::vec3& rd, Triangle& triangleOut, float& distOut)
{
	bool res = false;

	distOut = float(0xFFFF);

	GPUBVHNode* stack[64];
	GPUBVHNode** pStack = stack;
	*pStack++ = NULL;

	GPUBVHNode* currNode = bvh;
	int stackIndex = 1;
	do
	{
		if (IntersectBox(ro, rd, currNode->box.min, currNode->box.max))
		{
			if (currNode->rightOffset == -1) // leaf
			{
				if (glm::dot(rd, currNode->triangle.normal) != 0.0f)
				{
					float d = glm::dot((currNode->triangle.v1 - ro), currNode->triangle.normal) / glm::dot(rd, currNode->triangle.normal);
					if (d >= 0)
					{
						glm::vec3 p = ro + rd * d;
						if (IsInside(p, currNode->triangle.v1, currNode->triangle.v2, currNode->triangle.v3))
						{
							if (d < distOut)
							{
								distOut = d;
								triangleOut = currNode->triangle;
							}
							res = true;
						}
					}
				}
				currNode = *--pStack;
				stackIndex--;
			}
			else // interier
			{
				GPUBVHNode* left = &(bvh[currNode->nodeIndex + 1]);
				GPUBVHNode* right = &(bvh[currNode->nodeIndex + currNode->rightOffset]);
				currNode = left;
				*pStack++ = right;
				stackIndex++;
			}
		}
		else
		{
			currNode = *--pStack;
			stackIndex--;
		}
	} while (stackIndex > 0 && stackIndex < 64);

	return res;
}

__device__ glm::vec2 GetUV(const glm::vec3& p, const Triangle& t)
{
	glm::vec3 v2 = p - t.v1;
	float d20 = glm::dot(v2, t.barycentricInfo.v0);
	float d21 = glm::dot(v2, t.barycentricInfo.v1);

	float alpha = (t.barycentricInfo.d11 * d20 - t.barycentricInfo.d01 * d21) *
		t.barycentricInfo.invDenom;
	float beta = (t.barycentricInfo.d00 * d21 - t.barycentricInfo.d01 * d20) *
		t.barycentricInfo.invDenom;

	return (1.0f - alpha - beta) * t.uv1 + alpha * t.uv2 + beta * t.uv3;
}

__device__ glm::vec3 GetSmoothNormal(const glm::vec3& p, const Triangle& t)
{
	glm::vec3 v2 = p - t.v1;
	float d20 = glm::dot(v2, t.barycentricInfo.v0);
	float d21 = glm::dot(v2, t.barycentricInfo.v1);

	float alpha = (t.barycentricInfo.d11 * d20 - t.barycentricInfo.d01 * d21) *
		t.barycentricInfo.invDenom;
	float beta = (t.barycentricInfo.d00 * d21 - t.barycentricInfo.d01 * d20) *
		t.barycentricInfo.invDenom;

	glm::vec3 n = (1.0f - alpha - beta) * t.n1 + alpha * t.n2 + beta * t.n3;
	glm::vec3 res = glm::normalize(glm::vec3(n.x, -n.y, n.z));
	return glm::normalize(n);
}

__device__ glm::vec3 reflect(glm::vec3 I, glm::vec3 N)
{
	return I - N * glm::dot(N, I) * glm::vec3(2);
}

__device__ glm::vec3 Trace(glm::vec3 ro, glm::vec3 rd, int& depth, bool& inside, curandState_t& state)
{
	float d = 0.0f;
	Triangle t;
	if (Hit(ro, rd, t, d))
	{
		Material& mat = t.material;
		glm::vec3 p = ro + rd * d;
		glm::vec2 uv = GetUV(p, t);
		glm::vec3 n = t.normal;
		if (t.smoothing)
			n = GetSmoothNormal(p, t);
		if (glm::dot(n, rd) > 0.0f)
			n = -n;
		if (mat.normalTexId != -1)
		{
			glm::mat3 TBN = glm::mat3(t.tangent, t.bitangent, n);
			glm::vec3 nt = glm::vec3(CUDATex2D(textures[mat.normalTexId], uv)) * 2.0f - 1.0f;
			if (nt.z < 0.0f)
				nt = glm::vec3(nt.x, nt.y, 0.0f);
			nt = glm::normalize(nt);
			n = glm::normalize(TBN * nt);
		}
		p += n * EPSILON;

		if (depth < maxDepth * 2)
		{
			depth++;
			// Russian Roulette Path Termination
			float prob = glm::min(0.95f, glm::max(glm::max(mat.baseColor.x, mat.baseColor.y), mat.baseColor.z));
			if (depth >= maxDepth)
			{
				if (fabs(curand_uniform(&state)) > prob)
					return mat.emissive * mat.emissiveIntensity;
			}

			glm::vec3 r = reflect(rd, n);
			glm::vec3 reflectDir = r;
			if (mat.type == MaterialType::SPECULAR)
				reflectDir = r;
			else if (mat.type == MaterialType::DIFFUSE)
			{
				// Monte Carlo Integration
				glm::vec3 u = glm::abs(n.x) < 1.0f - EPSILON ? glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), n) : glm::cross(glm::vec3(1.0f), n);
				u = glm::normalize(u);
				glm::vec3 v = glm::normalize(glm::cross(u, n));
				float w = curand_uniform(&state), theta = curand_uniform(&state);
				// uniformly sampling on hemisphere
				reflectDir = w * cosf(2.0f * PI * theta) * u + w * sinf(2.0f * PI * theta) * v + glm::sqrt(1.0f - w * w) * n;
				reflectDir = glm::normalize(reflectDir);
			}
			else if (mat.type == MaterialType::GLOSSY)
			{
				// Monte Carlo Integration
				glm::vec3 u = fabs(n.x) < 1 - FLT_EPSILON ? glm::cross(glm::vec3(1, 0, 0), r) : glm::cross(glm::vec3(1), r);
				u = glm::normalize(u);
				glm::vec3 v = glm::cross(u, r);
				float w = curand_uniform(&state) * mat.roughness, theta = curand_uniform(&state);
				// wighted sampling on hemisphere
				reflectDir = w * cosf(2 * PI * theta) * u + w * sinf(2 * PI * theta) * v + sqrtf(1 - w * w) * r;
			}
			else if (mat.type == MaterialType::GLASS)
			{
				float nc = 1.0f, ng = 1.5f;
				// Snells law
				float eta = inside ? ng / nc : nc / ng;
				float r0 = (nc - ng) / (nc + ng);
				r0 = r0 * r0;
				float c = fabs(glm::dot(rd, n));
				float k = 1.0f - eta * eta * (1.0f - c * c);
				if (k < 0.0f)
					reflectDir = r;
				else
				{
					// Shilick's approximation of Fresnel's equation
					float re = r0 + (1.0f - r0) * (1.0f - c) * (1.0f - c);
					if (fabs(curand_uniform(&state)) < re)
						reflectDir = r;
					else
					{
						reflectDir = glm::normalize(eta * rd - (eta * glm::dot(n, rd) + sqrtf(k)) * n);
						p -= n * EPSILON * 2.0f;
						inside = !inside;
					}
				}
			}
			
			return mat.emissive * mat.emissiveIntensity + Trace(p, reflectDir, depth, inside, state) * mat.baseColor;
		}
	}

	return glm::vec3(0.0f);
}

__global__ void RenderPixel(float* img)
{
	const int x = threadIdx.x + blockIdx.x * blockDim.x;
	const int y = threadIdx.y + blockIdx.y * blockDim.y;
	if (x >= resX || y >= resY)
		return;
	const int index = x + (resY - y - 1)*resX;
	curandState_t localState = state[index];

	// Position world space image plane
	glm::vec3 camPos = glm::vec3(_camPos[0], _camPos[1], _camPos[2]);
	glm::vec3 camDir = glm::vec3(_camDir[0], _camDir[1], _camDir[2]);
	glm::vec3 camUp = glm::vec3(_camUp[0], _camUp[1], _camUp[2]);
	glm::vec3 imgCenter = camPos + camDir * camFocal;
	float imgHeight = 2.0f * camFocal * tan((camFovy / 2.0f) * PI / 180.0f);
	float aspect = (float)resX / (float)resY;
	float imgWidth = imgHeight * aspect;
	float deltaX = imgWidth / (float)resX;
	float deltaY = imgHeight / (float)resY;
	glm::vec3 camRight = glm::normalize(glm::cross(camUp, camDir));

	// Starting at top left
	glm::vec3 topLeft = imgCenter - camRight * (imgWidth * 0.5f);
	topLeft += camUp * (imgHeight * 0.5f);
	glm::vec3 pixel = topLeft - camUp * (float(y) * deltaY) + camRight * (float(x) * deltaX);

	glm::vec3 rayDir = glm::normalize(pixel - camPos);
	int depth = 0;
	bool inside = false;
	glm::vec3 color = Trace(camPos, rayDir, depth, inside, localState);

	// Draw
	glm::vec3 preColor = glm::vec3();
	memcpy(&preColor[0], img + 3 * index, 3 * sizeof(float));
	color = (preColor * float(samples - 1) + color) / float(samples);
	color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
	memcpy(img + 3 * index, &color[0], 3 * sizeof(float));
	state[index] = localState;
}

void CUDARenderFrame(int w, int h, float* img, int& h_samples)
{
	gpuErrchk(cudaMemcpyToSymbol(samples, &h_samples, sizeof(int)));
	dim3 blockDim(16, 16, 1), gridDim(w / blockDim.x + 1, h / blockDim.y + 1, 1);
	RenderPixel << <gridDim, blockDim >> > (img);
	gpuErrchk(cudaGetLastError());
	gpuErrchk(cudaDeviceSynchronize());
}

void CUDAReset()
{
	if (bvh != 0)
	{
		gpuErrchk(cudaFree(bvh));
		bvh = 0;
	}

	if (textures != 0)
	{
		for (int i = 0; i < numTextures; i++)
			gpuErrchk(cudaFree(textures[i].data));
		gpuErrchk(cudaFree(textures));
		textures = 0;
		numTextures = 0;
	}
}

void CUDAFinish()
{
	CUDAReset();
	if (state)
		gpuErrchk(cudaFree(state));
}
