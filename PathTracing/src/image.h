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
	unsigned char* mData;

public:
	Image();
	Image(std::string filename);
	~Image();

public:
	int width();
	int height();

	void Load(std::string filename);

	glm::vec4 tex2D(glm::vec2 uv);
	unsigned char* data();
};

#endif
