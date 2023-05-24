#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

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
	mData = stbi_load(filename.c_str(), &mWidth, &mHeight, &n, 4);

	if (mData && (mWidth > 1024 || mHeight > 1024))
	{
		float scale = 1024.f / fmax(mWidth, mHeight);
		int newWidth = mWidth * scale;
		int newHeight = mHeight * scale;
		size_t newSize = newWidth * newHeight * 4 * sizeof(unsigned char);
		unsigned char* newData = (unsigned char*)malloc(newSize);
		stbir_resize_uint8(mData, mWidth, mHeight, 0,
			newData, newWidth, newHeight, 0, 4);
		stbi_image_free(mData);
		mWidth = newWidth;
		mHeight = newHeight;
		mData = newData;
	}
}

glm::vec4 Image::tex2D(const glm::vec2& uv)
{
	if (!mData)
		return glm::vec4(0.0f);

	float u = fmod(uv.x, 1.0f);
	float v = fmod(uv.y, 1.0f);

	if (u < 0.0f)
		u += 1.0f;
	if (v < 0.0f)
		v += 1.0f;

	glm::ivec2 coord = glm::ivec2(mWidth * u, mHeight * v);
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
