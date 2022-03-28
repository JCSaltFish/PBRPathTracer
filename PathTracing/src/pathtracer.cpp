#define _USE_MATH_DEFINES
#include <math.h>
#include <omp.h>

#include "pathtracer.h"
#include "OBJ_Loader.h"

PathTracer::PathTracer()
{
	m_imgResolution = glm::ivec2(0);
	m_outImg = 0;

	m_camDir = glm::vec3(0.0f, 0.0f, 1.0f);
	m_camUp = glm::vec3(0.0f, 1.0f, 0.0f);
	m_camFocal = 0.1f;
	m_camFovy = 90;
}

PathTracer::~PathTracer()
{

}

void PathTracer::LoadMesh(std::string file, glm::mat4 model)
{
	objl::Loader loader;
	if (loader.LoadFile(file))
	{
		for (int i = 0; i < loader.LoadedIndices.size(); i += 3)
		{
			MeshTriangle t;
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
			glm::vec3 e1 = t.v2 - t.v1;
			glm::vec3 e2 = t.v3 - t.v2;
			t.normal = glm::normalize(glm::cross(e1, e2));

			m_scene.push_back(t);
		}
	}
}

void PathTracer::SetOutImage(GLubyte* out)
{
	m_outImg = out;
}

void PathTracer::SetResolution(glm::ivec2 res)
{
	m_imgResolution = res;
}

glm::ivec2 PathTracer::GetResolution()
{
	return m_imgResolution;
}

void PathTracer::SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up)
{
	m_camPos = pos;
	m_camDir = glm::normalize(dir);
	m_camUp = glm::normalize(up);
}

void PathTracer::SetProjection(float f, float fovy)
{
	m_camFocal = f;
	if (m_camFocal <= 0.0f)
		m_camFocal = 0.1f;
	m_camFovy = fovy;
	if (m_camFovy <= 0.0f)
		m_camFovy = 0.1f;
	else if (m_camFovy >= 180.0f)
		m_camFovy = 179.5;
}

bool PathTracer::Intersect(glm::vec3 ro, glm::vec3 rd, MeshTriangle t, float& distOut)
{
	bool res = false;
	if (glm::dot(rd, t.normal) == 0.0f)
		return res;
	distOut = glm::dot((t.v1 - ro), t.normal) / glm::dot(rd, t.normal);
	if (distOut < 0)
		return res;
	glm::vec3 p = ro + rd * distOut;

	// If the sum of all the angles is approximately equal to 360 degrees
	// then the intersection is in the triangle.
	float r = glm::acos(glm::dot(glm::normalize(t.v1 - p), glm::normalize(t.v2 - p)));
	r += glm::acos(glm::dot(glm::normalize(t.v2 - p), glm::normalize(t.v3 - p)));
	r += glm::acos(glm::dot(glm::normalize(t.v3 - p), glm::normalize(t.v1 - p)));
	if (abs(r - 2.0f * (float)M_PI) < EPS)
		res = true;

	return res;
}

glm::vec3 PathTracer::Trace(glm::vec3 ro, glm::vec3 rd)
{
	float dist = INF;
	int index = -1;
	for (int i = 0; i < m_scene.size(); i++)
	{
		float d = 0.0f;
		if (Intersect(ro, rd, m_scene[i], d))
		{
			if (d < dist)
			{
				dist = d;
				index = i;
			}
		}
	}

	if (index != -1)
	{
		return glm::vec3(1.0f);
	}

	return glm::vec3(0.0f);
}

void PathTracer::RenderFrame()
{
	// Position world space image plane
	glm::vec3 imgCenter = m_camPos + m_camDir * m_camFocal;
	float imgHeight = 2.0f * m_camFocal * tan((m_camFovy / 2.0f) * M_PI / 180.0f);
	float aspect = (float)m_imgResolution.x / (float)m_imgResolution.y;
	float imgWidth = imgHeight * aspect;
	float deltaX = imgWidth / (float)m_imgResolution.x;
	float deltaY = imgHeight / (float)m_imgResolution.y;
	glm::vec3 camRight = glm::normalize(glm::cross(m_camUp, m_camDir));

	// Starting at top left
	glm::vec3 topLeft = imgCenter - camRight * (imgWidth * 0.5f);
	topLeft += m_camUp * (imgHeight * 0.5f);
	// Loop through each pixel
	int numThreads = omp_get_max_threads();
	if (numThreads > 2)
		numThreads -= 3;
	else if (numThreads > 1)
		numThreads -= 2;
	else if (numThreads > 0)
		numThreads--;
	#pragma omp parallel for num_threads(numThreads)
	for (int i = 0; i < m_imgResolution.y; i++)
	{
		glm::vec3 pixel = topLeft - m_camUp * ((float)i * deltaY);
		for (int j = 0; j < m_imgResolution.x; j++)
		{
			glm::vec3 rayDir = glm::normalize(pixel - m_camPos);
			// TODO Trace & draw here
			glm::vec3 color = Trace(m_camPos, rayDir);
			// Draw
			m_outImg[((m_imgResolution.y - 1 - i) * m_imgResolution.x + j) * 3] = color.r * 255;
			m_outImg[((m_imgResolution.y - 1 - i) * m_imgResolution.x + j) * 3 + 1] = color.g * 255;
			m_outImg[((m_imgResolution.y - 1 - i) * m_imgResolution.x + j) * 3 + 2] = color.b * 255;

			pixel += camRight * deltaX;
		}
	}
}
