#ifndef __PATHTRACER_H__
#define __PATHTRACER_H__

#include <GL/glew.h>
#include <glm/glm.hpp>

class PathTracer
{
private:
	glm::ivec2 m_imgResolution;
	GLubyte* m_outImg;

	glm::vec3 m_camPos;
	glm::vec3 m_camDir;
	glm::vec3 m_camUp;
	float m_camFocal;
	float m_camFovy;

public:
	PathTracer();
	~PathTracer();

public:
	void SetOutImage(GLubyte* out);
	void SetResolution(glm::ivec2 res);
	glm::ivec2 GetResolution();
	void SetCamera(glm::vec3 pos, glm::vec3 dir, glm::vec3 up);
	void SetProjection(float f, float fovy);
	void RenderFrame();
};

#endif
