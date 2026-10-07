#ifndef __DISABLED_DEFAULT_TEXTURE_LOADER__
#include "EffekseerRenderer.PngTextureLoader.h"
#include <chrono>
#include <limits>

#ifdef __EFFEKSEER_USE_LIBPNG__
#include <png.h>
#else
#define STB_IMAGE_EFFEKSEER_IMPLEMENTATION
// stb's x86 SIMD path requires SSE2; disable it only for 32-bit x86 targets without SSE2.
#if !defined(STBI_NO_SIMD) &&                                          \
	((defined(_M_IX86) && (!defined(_M_IX86_FP) || _M_IX86_FP < 2)) || \
	 ((defined(__i386) || defined(__i386__)) && !defined(__SSE2__)))
#define STBI_NO_SIMD
#endif
#include "../3rdParty/stb_effekseer/stb_image_effekseer.h"

#endif

namespace EffekseerRenderer
{
#ifdef __EFFEKSEER_USE_LIBPNG__
struct PngReadContext
{
	const uint8_t* current = nullptr;
	const uint8_t* end = nullptr;
};

static void PngReadData(png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto context = static_cast<PngReadContext*>(png_get_io_ptr(png_ptr));
	if (context == nullptr || length > static_cast<size_t>(context->end - context->current))
	{
		png_error(png_ptr, "Unexpected end of PNG data");
		return;
	}
	memcpy(data, context->current, length);
	context->current += length;
}
#endif

bool PngTextureLoader::Load(const void* data, int32_t size, bool rev)
{
	if (data == nullptr || size <= 0)
		return false;
#ifdef __EFFEKSEER_USE_LIBPNG__
	textureWidth = 0;
	textureHeight = 0;
	textureData.clear();

	PngReadContext readContext{static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size};

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if (png == nullptr)
		return false;
	png_set_read_fn(png, &readContext, &PngReadData);

	png_infop png_info = png_create_info_struct(png);
	if (png_info == nullptr)
	{
		png_destroy_read_struct(&png, nullptr, nullptr);
		return false;
	}

	if (setjmp(png_jmpbuf(png)))
	{
		png_destroy_read_struct(&png, &png_info, nullptr);
		return false;
	}

	png_read_info(png, png_info);

	const auto interlaceType = png_get_interlace_type(png, png_info);

	int passes = 1;
	if (interlaceType != PNG_INTERLACE_NONE)
		passes = png_set_interlace_handling(png);

	const png_byte bit_depth = png_get_bit_depth(png, png_info);
	if (bit_depth < 8)
	{
		png_set_packing(png);
	}
	else if (bit_depth == 16)
	{
		png_set_strip_16(png);
	}

	uint32_t pixelBytes = 4;
	const png_byte color_type = png_get_color_type(png, png_info);
	switch (color_type)
	{
	case PNG_COLOR_TYPE_PALETTE:
	{
		png_set_palette_to_rgb(png);

		png_bytep trans_alpha = nullptr;
		int num_trans = 0;
		png_color_16p trans_color = nullptr;

		png_get_tRNS(png, png_info, &trans_alpha, &num_trans, &trans_color);
		if (trans_alpha != nullptr)
		{
			pixelBytes = 4;
		}
		else
		{
			pixelBytes = 3;
		}
	}
	break;
	case PNG_COLOR_TYPE_GRAY:
		pixelBytes = 1;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		png_set_gray_to_rgb(png);
		pixelBytes = 4;
		break;
	case PNG_COLOR_TYPE_RGB:
		pixelBytes = 3;
		break;
	case PNG_COLOR_TYPE_RGBA:
		break;
	}

	textureWidth = png_get_image_width(png, png_info);
	textureHeight = png_get_image_height(png, png_info);
	const size_t pixelCount = static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight);
	if (textureWidth <= 0 || textureHeight <= 0 || pixelCount > std::numeric_limits<size_t>::max() / 4 ||
		pixelCount > std::numeric_limits<size_t>::max() / pixelBytes)
	{
		png_destroy_read_struct(&png, &png_info, nullptr);
		return false;
	}

	uint8_t* image = new uint8_t[pixelCount * pixelBytes];
	size_t pitch = static_cast<size_t>(textureWidth) * pixelBytes;

	for (int pass = 0; pass < passes; pass++)
	{
		if (rev)
		{
			for (int32_t i = 0; i < textureHeight; i++)
			{
				png_read_row(png, &image[(textureHeight - 1 - i) * pitch], nullptr);
			}
		}
		else
		{
			for (int32_t i = 0; i < textureHeight; i++)
			{
				png_read_row(png, &image[i * pitch], nullptr);
			}
		}
	}

	textureData.resize(pixelCount * 4);
	auto imagedst_ = textureData.data();

	if (pixelBytes == 4)
	{
		memcpy(imagedst_, image, pixelCount * 4);
	}
	else if (pixelBytes == 1)
	{
		for (int32_t y = 0; y < textureHeight; y++)
		{
			for (int32_t x = 0; x < textureWidth; x++)
			{
				const auto pixelIndex = static_cast<size_t>(x) + static_cast<size_t>(y) * textureWidth;
				auto src = pixelIndex;
				auto dst = pixelIndex * 4;
				imagedst_[dst + 0] = image[src + 0];
				imagedst_[dst + 1] = image[src + 0];
				imagedst_[dst + 2] = image[src + 0];
				imagedst_[dst + 3] = 255;
			}
		}
	}
	else
	{
		for (int32_t y = 0; y < textureHeight; y++)
		{
			for (int32_t x = 0; x < textureWidth; x++)
			{
				const auto pixelIndex = static_cast<size_t>(x) + static_cast<size_t>(y) * textureWidth;
				auto src = pixelIndex * 3;
				auto dst = pixelIndex * 4;
				imagedst_[dst + 0] = image[src + 0];
				imagedst_[dst + 1] = image[src + 1];
				imagedst_[dst + 2] = image[src + 2];
				imagedst_[dst + 3] = 255;
			}
		}
	}

	delete[] image;
	png_destroy_read_struct(&png, &png_info, nullptr);

	return true;

#else

	unsigned char* pixels = nullptr;
	int width = 0;
	int height = 0;
	int bpp = 0;

	auto pre = std::chrono::high_resolution_clock::now();

	pixels = (uint8_t*)Effekseer::stbi_load_from_memory((Effekseer::stbi_uc const*)data, size, &width, &height, &bpp, 0);

	if (pixels != nullptr && width > 0 && height > 0 &&
		static_cast<size_t>(width) <= std::numeric_limits<size_t>::max() / static_cast<size_t>(height) &&
		static_cast<size_t>(width) * static_cast<size_t>(height) <= std::numeric_limits<size_t>::max() / 4)
	{
		const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
		textureData.resize(pixelCount * 4);
		textureWidth = width;
		textureHeight = height;
		auto buf = textureData.data();

		if (bpp == 4)
		{
			memcpy(textureData.data(), pixels, pixelCount * 4);
		}
		else if (bpp == 2)
		{
			// Gray+Alpha
			for (int h = 0; h < height; h++)
			{
				for (int w = 0; w < width; w++)
				{
					const size_t index = static_cast<size_t>(w) + static_cast<size_t>(h) * width;
					((uint8_t*)buf)[index * 4 + 0] = pixels[index * 2 + 0];
					((uint8_t*)buf)[index * 4 + 1] = pixels[index * 2 + 0];
					((uint8_t*)buf)[index * 4 + 2] = pixels[index * 2 + 0];
					((uint8_t*)buf)[index * 4 + 3] = pixels[index * 2 + 1];
				}
			}
		}
		else if (bpp == 1)
		{
			// Gray
			for (int h = 0; h < height; h++)
			{
				for (int w = 0; w < width; w++)
				{
					const size_t index = static_cast<size_t>(w) + static_cast<size_t>(h) * width;
					((uint8_t*)buf)[index * 4 + 0] = pixels[index];
					((uint8_t*)buf)[index * 4 + 1] = pixels[index];
					((uint8_t*)buf)[index * 4 + 2] = pixels[index];
					((uint8_t*)buf)[index * 4 + 3] = 255;
				}
			}
		}
		else
		{
			for (int h = 0; h < height; h++)
			{
				for (int w = 0; w < width; w++)
				{
					const size_t index = static_cast<size_t>(w) + static_cast<size_t>(h) * width;
					((uint8_t*)buf)[index * 4 + 0] = pixels[index * 3 + 0];
					((uint8_t*)buf)[index * 4 + 1] = pixels[index * 3 + 1];
					((uint8_t*)buf)[index * 4 + 2] = pixels[index * 3 + 2];
					((uint8_t*)buf)[index * 4 + 3] = 255;
				}
			}
		}

		Effekseer::stbi_image_free(pixels);
		return true;
	}

	Effekseer::stbi_image_free(pixels);
	return false;
#endif
}

void PngTextureLoader::Unload()
{
	textureData.clear();
}

} // namespace EffekseerRenderer

#endif
