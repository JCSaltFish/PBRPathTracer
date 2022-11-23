#ifndef __CUDAKERNEL_H__
#define __CUDAKERNEL_H__

#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include <glm/glm.hpp>

#include "mesh.h"

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char* file, int line, bool abort = true)
{
	if (code != cudaSuccess)
	{
		fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
		if (abort)
			exit(code);
	}
}

void InitCUDA();

void CUDASetResolution(int x, int y);
void CUDASetTraceDepth(int depth);
void CUDASetCamera(float* pos, float* dir, float* up);
void CUDASetProjection(float f, float fovy);
void CUDASetBVH(GPUBVHNode* nodes, int size);
void CUDALoadTextures(const std::vector<Image*>& texVec);

void CUDARenderFrame(int w, int h, float* img, int& h_samples);

void CUDAReset();

#endif