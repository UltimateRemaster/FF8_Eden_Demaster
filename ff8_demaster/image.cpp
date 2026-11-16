#include "image.h"

#include <filesystem>
#include <fstream>
#include "debug.h"
#include <bimg/bimg.h>

#include "coreHeader.h"
#include "opengl.h"
#include "upload_levels.h"

SafeBimg SafeBimgInit(bimg::ImageContainer* img) { return {img, bimg::imageFree}; }

SafeBimg LoadImageFromFile(const char* const filename)
{
	CheckGlew();
	char msg[1024]{0};
	OutputDebug("%s - Opening File: %s\n", __func__, filename);
	
	std::fstream fp = std::fstream(filename, std::ios::in | std::ios::binary);
	if (!fp.is_open())
		return SafeBimgInit();

	fp.seekg(0, std::ios::end);
	const std::streampos filesize = fp.tellg();
	const std::unique_ptr<char[]> buffer = std::make_unique<char[]>(filesize);

	if (!buffer)
		return SafeBimgInit();

	fp.seekg(0, std::ios::beg);
	fp.read(buffer.get(), filesize);

	// REQUIRED BY BIMG FILE DECODING
	static bx::DefaultAllocator texAllocator{};
	bx::Error error{};
	constexpr static bimg::TextureFormat::Enum format{bimg::TextureFormat::Enum::Count};
	bimg::ImageContainer* img = bimg::imageParse(&texAllocator, buffer.get(), static_cast<uint32_t>(filesize)
		,format, &error);
	
	if (error.isOk())
		return SafeBimgInit(img);
	
	OutputDebug("%s::%d::bimg error: %s ", __func__, __LINE__, error.getMessage().getPtr());
	return SafeBimgInit();
}

// An RGBA8 image of given pixels in this Demaster's bimg allocator, owned like LoadImageFromFile's result (its deleter
// frees it through the allocator the container records): demaster_extension.h, createImageRgba8.
static bx::DefaultAllocator edenImageAllocator{};
SafeBimg CreateImageRgba8(const uint32_t width, const uint32_t height, const void* const rgba8)
{
	if (!width || !height || width > 0xFFFF || height > 0xFFFF || !rgba8)
		return SafeBimgInit();
	return SafeBimgInit(bimg::imageAlloc(&edenImageAllocator, bimg::TextureFormat::RGBA8, static_cast<uint16_t>(width),
		static_cast<uint16_t>(height), 0, 1, false, false, rgba8));
}

//appends DDS checks if file exists and then checks for PNG. returns false if atleast one exists. true on failure.
bool DDSorPNG(char* buffer, const size_t inSize, const char* fmt, ...)
{
	const size_t size = inSize - 4U; //for extension
	va_list args;
	va_start(args, fmt);
	int argLength = vsnprintf(buffer, size, fmt, args);
	strcat(buffer, ".dds");
	va_end(args);
	if (!std::filesystem::exists(buffer))
	{
		va_start(args, fmt);
		vsnprintf(buffer, size, fmt, args);
		strcat(buffer, ".png");
		va_end(args);
		return !std::filesystem::exists(buffer); //if it exist, then it should return false on success, right?
	}
	return false;
}
void RenderTexture(const bimg::ImageContainer* img)
{
	const TextureFormatInfo& texInfo = s_textureFormat[img->m_format];
	if (isCompressed(img->m_format))
		RenderCompressedTexture(img, texInfo);
	else
		RenderUncompressedTexture(img, texInfo);
}

void RenderUncompressedTexture(const bimg::ImageContainer* img, const TextureFormatInfo& texInfo)
{
	CheckGlew();
	// Every level with its own size (upload_levels.h); the loop sent the size of level 0 for all of them.
	demaster::SendLevels(img->m_width, img->m_height, img->m_numMips,
		[img](uint32_t lod, const void*& data, uint32_t& size)
		{
			bimg::ImageMip mip{};
			if (!imageGetRawData(*img, 0, static_cast<uint8_t>(lod), img->m_data, img->m_size, mip))
				return false;
			data = mip.m_data;
			size = mip.m_size;
			return true;
		},
		[&texInfo](uint32_t lod, uint32_t width, uint32_t height, const void* data, uint32_t)
		{
			glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(lod), static_cast<GLint>(texInfo.m_internalFmt)
				, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0
				, texInfo.m_fmt, texInfo.m_type, data);
		});
}

void RenderCompressedTexture(const bimg::ImageContainer* img, const TextureFormatInfo& texInfo)
{
	if (!GLEW_ARB_texture_compression)
	{
		OutputDebug("Texture is compressed, but compression is not supported on your GPU. Skipping draw.");
		return;
	}
	// Every level with its own size (upload_levels.h); the loop sent the size of level 0 for all of them, and GL refused
	// every level past the first (its byte count is the level's, not level 0's).
	demaster::SendLevels(img->m_width, img->m_height, img->m_numMips,
		[img](uint32_t lod, const void*& data, uint32_t& size)
		{
			bimg::ImageMip mip{};
			if (!bimg::imageGetRawData(*img, 0, static_cast<uint8_t>(lod), img->m_data, img->m_size, mip))
				return false;
			data = mip.m_data;
			size = mip.m_size;
			return true;
		},
		[&texInfo](uint32_t lod, uint32_t width, uint32_t height, const void* data, uint32_t size)
		{
			glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(lod), texInfo.m_internalFmt,
			                       static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
			                       static_cast<GLsizei>(size), data);
		});
}

void LoadAndRenderTexture(const char* const filename, const bool bAppendPath)
{
	std::string localPath;
	if (bAppendPath)
	{
		localPath = DIRECT_IO_EXPORT_DIR;
		localPath.append(filename);
	}
	else
		localPath = filename;

	const SafeBimg img = LoadImageFromFile(localPath.c_str());
	RenderTexture(img.get());
}

