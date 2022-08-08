#include "stb_image.h"

#include "image.h"

Image::Image() :
	mWidth(0),
	mHeight(0)
{
	mFilename = "";
	mData = 0;
}

Image::Image(std::string filename)
{
	mFilename = filename;
	mData = 0;
	Load(mFilename);
}

Image::~Image()
{
	if (mData)
		stbi_image_free(mData);
}

int Image::width()
{
	return mWidth;
}

int Image::height()
{
	return mHeight;
}

void Image::Load(std::string filename)
{
	if (mData)
		stbi_image_free(mData);

	mFilename = filename;
	int n;
	mData = stbi_load(filename.c_str(), &mWidth, &mHeight, &n, 4);
}

glm::vec4 Image::tex2D(glm::vec2 uv)
{
	if (!mData)
		return glm::vec4(0.0f);

	if (uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f)
		return glm::vec4(0.0f);

	glm::ivec2 coord = glm::ivec2(mWidth * uv.x, mHeight * uv.y);
	const unsigned char* p = mData + (4 * (coord.y * mWidth + coord.x));
	
	float r = (float)p[0] / 255.0f;
	float g = (float)p[1] / 255.0f;
	float b = (float)p[2] / 255.0f;
	float a = (float)p[3] / 255.0f;

	glm::vec4 res = glm::vec4(r, g, b, a);
	return res;
}

unsigned char* Image::data()
{
	return mData;
}
