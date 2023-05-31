#define _USE_MATH_DEFINES
#include <sstream>
#include <math.h>

#include <tiny_obj_loader.h>

#include "pathtracer.h"

PathTracer::PathTracer()
{
	mMaxDepth = 3;

	mCamDir = glm::vec3(0.0f, 0.0f, 1.0f);
	mCamUp = glm::vec3(0.0f, 1.0f, 0.0f);
	mCamFocal = 0.05f;
	mCamFovy = 90.0f;

	mCamFocalDist = 5.0f;
	mCamAperture = 0.0f;

	mSamples = 0;
	mNeedReset = false;
	mExit = false;

	m_GPUOutImage = -1;
	m_GPUProgram = -1;

	m_GPUMaterialsSSBO = -1;
	m_GPUTrianglesSSBO = -1;
	m_GPUBVHSSBO = -1;
	m_GPULightsourcesSSBO = -1;
	m_GPUTexSizesSSBO = -1;

	m_GPUPathTracerUBO = -1;
	m_GPUCameraUBO = -1;

	m_GPUTextures = -1;
}

PathTracer::~PathTracer()
{
	ClearScene();
	GPUClearScene();
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

				t.id = mTriangles.size();

				GPUTriangle tri;
				tri.v1 = glm::vec4(t.v1, 1.f);
				tri.v2 = glm::vec4(t.v2, 1.f);
				tri.v3 = glm::vec4(t.v3, 1.f);
				tri.n1 = glm::vec4(t.n1, 1.f);
				tri.n2 = glm::vec4(t.n2, 1.f);
				tri.n3 = glm::vec4(t.n3, 1.f);
				tri.normal = glm::vec4(t.normal, 1.f);
				tri.tangent = glm::vec4(t.tangent, 1.f);
				tri.bitangent = glm::vec4(t.bitangent, 1.f);
				tri.uv1 = t.uv1;
				tri.uv2 = t.uv2;
				tri.uv3 = t.uv3;
				tri.smoothing = t.smoothing;
				tri.id = t.id;
				tri.objectId = t.objectId;
				tri.elementId = t.elementId;
				mTriangles.push_back(tri);
			}
		}
		mLoadedObjects.push_back(obj);
	}
}

void PathTracer::SetDiffuseTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.diffuseTexId != -1)
		mTextures[mat.diffuseTexId] = texture;
	else
	{
		mat.diffuseTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetNormalTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.normalTexId != -1)
		mTextures[mat.normalTexId] = texture;
	else
	{
		mat.normalTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetEmissTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.emissTexId != -1)
		mTextures[mat.emissTexId] = texture;
	else
	{
		mat.emissTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetRoughnessTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.roughnessTexId != -1)
		mTextures[mat.roughnessTexId] = texture;
	else
	{
		mat.roughnessTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetMetallicTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.metallicTexId != -1)
		mTextures[mat.metallicTexId] = texture;
	else
	{
		mat.metallicTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetOpacityTextureForElement(int objId, int elementId, GLuint texture)
{
	Material& mat = mLoadedObjects[objId].elements[elementId].material;
	if (mat.opacityTexId != -1)
		mTextures[mat.opacityTexId] = texture;
	else
	{
		mat.opacityTexId = mTextures.size();
		mTextures.push_back(texture);
	}
}

void PathTracer::SetMaterial(int objId, int elementId, Material& material)
{
	if (objId >= mLoadedObjects.size())
		return;
	if (elementId >= mLoadedObjects[objId].elements.size())
		return;

	material.diffuseTexId = mLoadedObjects[objId].elements[elementId].material.diffuseTexId;
	material.normalTexId = mLoadedObjects[objId].elements[elementId].material.normalTexId;
	material.emissTexId = mLoadedObjects[objId].elements[elementId].material.emissTexId;
	material.roughnessTexId = mLoadedObjects[objId].elements[elementId].material.roughnessTexId;
	material.metallicTexId = mLoadedObjects[objId].elements[elementId].material.metallicTexId;
	material.opacityTexId = mLoadedObjects[objId].elements[elementId].material.opacityTexId;

	mLoadedObjects[objId].elements[elementId].material = material;
}

void PathTracer::BuildBVH()
{
	if (mBvh)
		delete mBvh;
	mBvh = new BVHNode();
	mBvh->Construct(mTriangles, mTriangles.size());
}

void PathTracer::ResetImage()
{
	mNeedReset = true;
}

void PathTracer::ClearScene()
{
	mTriangles.swap(std::vector<GPUTriangle>());
	mLoadedObjects.swap(std::vector<PathTracerLoader::Object>());
	if (mBvh)
		delete mBvh;
	mBvh = 0;
	mTextures.swap(std::vector<GLuint>());
}

void PathTracer::SetResolution(const glm::ivec2& res)
{
	mResolution = res;
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

void PathTracer::Exit()
{
	mExit = true;
}

void PathTracer::GPUSetOutImage(GLuint texture)
{
	m_GPUOutImage = texture;
}

void PathTracer::GPUSetProgram(GLuint program)
{
	m_GPUProgram = program;
}

void PathTracer::GPUBuildScene()
{
	glUseProgram(m_GPUProgram);

	// 0. Materials
	std::vector<GPUMaterial> materials;
	for (auto& obj : mLoadedObjects)
	{
		for (auto& e : obj.elements)
		{
			GPUMaterial m;
			m.translucent = e.material.type == MaterialType::TRANSLUCENT;
			m.diffuseTexId = e.material.diffuseTexId;
			m.normalTexId = e.material.normalTexId;
			m.emissTexId = e.material.emissTexId;
			m.roughnessTexId = e.material.roughnessTexId;
			m.metallicTexId = e.material.metallicTexId;
			m.opacityTexId = e.material.opacityTexId;
			m.diffuse = glm::vec4(e.material.diffuse, 1.f);
			m.specular = glm::vec4(e.material.specular, 1.f);
			m.emissive = glm::vec4(e.material.emissive, 1.f);
			m.emissIntensity = e.material.emissiveIntensity;
			m.roughness = e.material.roughness;
			m.reflectiveness = e.material.reflectiveness;
			m.translucency = e.material.translucency;
			m.ior = e.material.ior;
			e.material.id = materials.size();
			materials.push_back(m);
		}
	}

	if (m_GPUMaterialsSSBO != -1)
		glDeleteBuffers(1, &m_GPUMaterialsSSBO);
	glGenBuffers(1, &m_GPUMaterialsSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GPUMaterialsSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(GPUMaterial), materials.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_GPUMaterialsSSBO);

	// Textures
	std::vector<glm::ivec2> sizes(mTextures.size());
	glm::ivec2 maxSize(0);
	for (int i = 0; i < mTextures.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, mTextures[i]);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &sizes[i].x);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &sizes[i].y);
		maxSize.x = std::max(maxSize.x, sizes[i].x);
		maxSize.y = std::max(maxSize.y, sizes[i].y);
	}

	if (m_GPUTextures != -1)
		glDeleteTextures(1, &m_GPUTextures);
	glGenTextures(1, &m_GPUTextures);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_GPUTextures);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGB8, maxSize.x, maxSize.y, mTextures.size());
	for (int i = 0; i < mTextures.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, mTextures[i]);
		glCopyImageSubData(mTextures[i], GL_TEXTURE_2D, 0, 0, 0, 0,
			m_GPUTextures, GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, sizes[i].x, sizes[i].y, 1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	if (m_GPUTexSizesSSBO != -1)
		glDeleteBuffers(1, &m_GPUTexSizesSSBO);
	glGenBuffers(1, &m_GPUTexSizesSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GPUTexSizesSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizes.size() * sizeof(glm::ivec2), sizes.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_GPUTexSizesSSBO);

	// 1. BVH
	std::vector<int> lightsources;

	for (auto& t : mTriangles)
	{
		auto& m = mLoadedObjects[t.objectId].elements[t.elementId].material;
		t.materialId = m.id;
		if (glm::length(m.emissive) >= EPS)
			lightsources.push_back(t.id);
	}

	if (m_GPUTrianglesSSBO != -1)
		glDeleteBuffers(1, &m_GPUTrianglesSSBO);
	glGenBuffers(1, &m_GPUTrianglesSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GPUTrianglesSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, mTriangles.size() * sizeof(GPUTriangle), mTriangles.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_GPUTrianglesSSBO);

	if (m_GPULightsourcesSSBO != -1)
		glDeleteBuffers(1, &m_GPULightsourcesSSBO);
	glGenBuffers(1, &m_GPULightsourcesSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GPULightsourcesSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, lightsources.size() * sizeof(int), lightsources.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_GPULightsourcesSSBO);
	glUniform1i(0, lightsources.size());

	BuildBVH();
	std::vector<GPUBVHNode> bvh;
	mBvh->GetGPULayout(bvh);
	mTriangles.swap(std::vector<GPUTriangle>());
	delete mBvh;
	mBvh = 0;

	if (m_GPUBVHSSBO != -1)
		glDeleteBuffers(1, &m_GPUBVHSSBO);
	glGenBuffers(1, &m_GPUBVHSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GPUBVHSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, bvh.size() * sizeof(GPUBVHNode), bvh.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_GPUBVHSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// 2. Path Tracer
	if (m_GPUPathTracerUBO != -1)
		glDeleteBuffers(1, &m_GPUPathTracerUBO);
	glGenBuffers(1, &m_GPUPathTracerUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_GPUPathTracerUBO);
	GPUPathTracerUniforms pathTracerUniforms;
	pathTracerUniforms.resolution = mResolution;
	pathTracerUniforms.maxDepth = mMaxDepth;
	pathTracerUniforms.samples = mSamples;
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUPathTracerUniforms), &pathTracerUniforms, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_GPUPathTracerUBO);

	// 3. Camera
	if (m_GPUCameraUBO != -1)
		glDeleteBuffers(1, &m_GPUCameraUBO);
	glGenBuffers(1, &m_GPUCameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_GPUCameraUBO);
	GPUCameraUniforms cameraUniforms;
	cameraUniforms.camPos = glm::vec4(mCamPos, 1.f);
	cameraUniforms.camDir = glm::vec4(mCamDir, 1.f);
	cameraUniforms.camUp = glm::vec4(mCamUp, 1.f);
	cameraUniforms.camFocal = mCamFocal;
	cameraUniforms.camFovy = mCamFovy;
	cameraUniforms.camFocalDist = mCamFocalDist;
	cameraUniforms.camAperture = mCamAperture;
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUCameraUniforms), &cameraUniforms, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_GPUCameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void PathTracer::GPURenderFrame()
{
	mExit = false;

	if (mNeedReset)
	{
		mNeedReset = false;
		mSamples = 0;
	}

	mSamples++;

	glUseProgram(m_GPUProgram);

	glBindBuffer(GL_UNIFORM_BUFFER, m_GPUPathTracerUBO);
	GPUPathTracerUniforms pathTracerUniforms;
	pathTracerUniforms.resolution = mResolution;
	pathTracerUniforms.maxDepth = mMaxDepth;
	pathTracerUniforms.samples = mSamples;
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUPathTracerUniforms), &pathTracerUniforms, GL_DYNAMIC_DRAW);

	glBindImageTexture(0, m_GPUOutImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_GPUTextures);
	glUniform1i(1, 1);

	GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	glDispatchCompute(ceil(float(mResolution.x) / 32.), ceil(float(mResolution.y) / 32.), 1);

	glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5000000000);
	glDeleteSync(fence);

	glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void PathTracer::GPUClearScene()
{
	if (m_GPUMaterialsSSBO != -1)
	{
		glDeleteBuffers(1, &m_GPUMaterialsSSBO);
		m_GPUMaterialsSSBO = -1;
	}
	if (m_GPUTrianglesSSBO != -1)
	{
		glDeleteBuffers(1, &m_GPUTrianglesSSBO);
		m_GPUTrianglesSSBO = -1;
	}
	if (m_GPUBVHSSBO != -1)
	{
		glDeleteBuffers(1, &m_GPUBVHSSBO);
		m_GPUBVHSSBO = -1;
	}
	if (m_GPULightsourcesSSBO != -1)
	{
		glDeleteBuffers(1, &m_GPULightsourcesSSBO);
		m_GPULightsourcesSSBO = -1;
	}
	if (m_GPUTexSizesSSBO != -1)
	{
		glDeleteBuffers(1, &m_GPUTexSizesSSBO);
		m_GPUTexSizesSSBO = -1;
	}

	if (m_GPUPathTracerUBO != -1)
	{
		glDeleteBuffers(1, &m_GPUPathTracerUBO);
		m_GPUPathTracerUBO = -1;
	}

	if (m_GPUCameraUBO != -1)
	{
		glDeleteBuffers(1, &m_GPUCameraUBO);
		m_GPUCameraUBO = -1;
	}

	if (m_GPUTextures != -1)
	{
		glDeleteTextures(1, &m_GPUTextures);
		m_GPUTextures = -1;
	}
}
