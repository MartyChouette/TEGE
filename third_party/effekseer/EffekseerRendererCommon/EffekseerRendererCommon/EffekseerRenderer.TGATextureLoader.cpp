
#ifndef __DISABLED_DEFAULT_TEXTURE_LOADER__

#include "EffekseerRenderer.TGATextureLoader.h"
#include "../../Effekseer/Effekseer/Utils/Effekseer.BinaryReader.h"
#include <limits>

namespace EffekseerRenderer
{

bool TGATextureLoader::Load(const void* data, int32_t size)
{
	const int TGA_HEADER_SIZE = 18;
	if (data == nullptr || size < 0)
		return false;
	Effekseer::BinaryReader<true> reader(static_cast<const uint8_t*>(data), static_cast<size_t>(size));
	std::array<uint8_t, TGA_HEADER_SIZE> TgaHeader{};
	if (!reader.Read(TgaHeader.data(), TGA_HEADER_SIZE))
		return false;

	textureWidth = TgaHeader[12] + TgaHeader[13] * 256;
	textureHeight = TgaHeader[14] + TgaHeader[15] * 256;

	int ColorStep{};

	if (TgaHeader[16] == 16)
	{
		ColorStep = 2;
	}
	else if (TgaHeader[16] == 24)
	{
		ColorStep = 3;
	}
	else if (TgaHeader[16] == 32)
	{
		ColorStep = 4;
	}
	else
	{
		return false;
	}

	// カラーマップ取得
	const size_t pixelCount = static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight);
	if (textureWidth <= 0 || textureHeight <= 0 || pixelCount > std::numeric_limits<size_t>::max() / 4)
		return false;
	const size_t sourceSize = pixelCount * static_cast<size_t>(ColorStep);
	if (!reader.CanRead(sourceSize))
		return false;
	textureData.resize(pixelCount * 4);
	const uint8_t* SrcTextureRef = reader.GetCurrentData();

	for (int h = 0; h < textureHeight; h++)
	{
		for (int w = 0; w < textureWidth; w++)
		{
			// 出力データ走査用(左上~)
			const size_t LU_Index = (static_cast<size_t>(h) * textureWidth + w) * 4;

			// 元データ走査用(左下~)
			const size_t LD_Index = ((static_cast<size_t>(textureHeight - 1 - h) * textureWidth) + w) * ColorStep;

			for (int c = 0; c < ColorStep; c++)
			{
				textureData[LU_Index + c] = SrcTextureRef[LD_Index + c];
			}

			if (ColorStep == 2)
			{
				textureData[LU_Index + 3] = textureData[LU_Index + 1];
				textureData[LU_Index + 1] = textureData[LU_Index + 0];
				textureData[LU_Index + 2] = textureData[LU_Index + 0];
			}

			if (ColorStep == 3)
			{
				textureData[LU_Index + 3] = 255;
			}
		}
	}

	// BGR -> RGBへ変換
	for (int h = 0; h < textureHeight; h++)
	{
		for (int w = 0; w < textureWidth; w++)
		{
			const size_t index = (static_cast<size_t>(h) * textureWidth + w) * 4;

			uint8_t tmp = textureData[index + 0];
			textureData[index + 0] = textureData[index + 2];
			textureData[index + 2] = tmp;
		}
	}

	return true;
}

void TGATextureLoader::Unload()
{
	textureData.clear();
}

void TGATextureLoader::Initialize()
{
}

void TGATextureLoader::Finalize()
{
}

} // namespace EffekseerRenderer

#endif
