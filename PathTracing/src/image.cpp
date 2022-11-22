#include "stb_image.h"

#include "image.h"

Image::Image() :
	mWidth(0),
	mHeight(0)
{
	mFilename = "";
	mData = 0;
}

Image::Image(const std::string& filename)
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

const int Image::width() const
{
	return mWidth;
}

const int Image::height() const
{
	return mHeight;
}

void Image::Load(const std::string& filename)
{
	if (mData)
		stbi_image_free(mData);

	mFilename = filename;
	int n;
	mData = stbi_loadf(filename.c_str(), &mWidth, &mHeight, &n, 4);
}

glm::vec4 Image::tex2D(const glm::vec2& uv)
{
	if (!mData)
		return glm::vec4(0.0f);

	if (uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f)
		return glm::vec4(0.0f);

	glm::ivec2 coord = glm::ivec2(mWidth * uv.x, mHeight * uv.y);
	float* p = mData + (4 * (coord.y * mWidth + coord.x));

	return glm::vec4(p[0], p[1], p[2], p[3]);
}

float* Image::data()
{
	return mData;
}
