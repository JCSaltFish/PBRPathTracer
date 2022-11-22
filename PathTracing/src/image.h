#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <string>
#include <glm/glm.hpp>

class Image
{
private:
	std::string mFilename;
	int mWidth;
	int mHeight;
	float* mData;

public:
	Image();
	Image(const std::string& filename);
	~Image();

public:
	const int width() const;
	const int height() const;

	void Load(const std::string& filename);

	glm::vec4 tex2D(const glm::vec2& uv);
	float* data();
};

#endif
