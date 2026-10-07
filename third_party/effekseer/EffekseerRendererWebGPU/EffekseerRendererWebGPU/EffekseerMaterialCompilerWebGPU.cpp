#include "EffekseerMaterialCompilerWebGPU.h"

#include "../../3rdParty/LLGI/src/WebGPU/LLGI.CompilerWebGPU.h"
#include "../../EffekseerMaterialCompiler/WGSLGenerator/ShaderGenerator.h"

#include <array>
#include <cstring>
#include <iostream>

namespace Effekseer
{
namespace
{
constexpr int32_t WebGPUModelRendererInstanceCount = 40;

void Serialize(std::vector<uint8_t>& dst, const LLGI::CompilerResult& result)
{
	uint32_t binarySize = sizeof(uint32_t);
	for (const auto& binary : result.Binary)
	{
		binarySize += sizeof(uint32_t);
		binarySize += static_cast<uint32_t>(binary.size());
	}

	dst.resize(binarySize);

	uint32_t offset = 0;
	const uint32_t count = static_cast<uint32_t>(result.Binary.size());
	memcpy(dst.data() + offset, &count, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	for (const auto& binary : result.Binary)
	{
		const uint32_t size = static_cast<uint32_t>(binary.size());
		memcpy(dst.data() + offset, &size, sizeof(uint32_t));
		offset += sizeof(uint32_t);

		memcpy(dst.data() + offset, binary.data(), binary.size());
		offset += static_cast<uint32_t>(binary.size());
	}
}

class CompiledMaterialBinaryWebGPU : public CompiledMaterialBinary, ReferenceObject
{
	std::array<std::vector<uint8_t>, static_cast<int32_t>(MaterialShaderType::Max)> vertexShaders_;
	std::array<std::vector<uint8_t>, static_cast<int32_t>(MaterialShaderType::Max)> pixelShaders_;

public:
	void SetVertexShaderData(MaterialShaderType type, const std::vector<uint8_t>& data)
	{
		vertexShaders_.at(static_cast<int32_t>(type)) = data;
	}

	void SetPixelShaderData(MaterialShaderType type, const std::vector<uint8_t>& data)
	{
		pixelShaders_.at(static_cast<int32_t>(type)) = data;
	}

	const uint8_t* GetVertexShaderData(MaterialShaderType type) const override
	{
		return vertexShaders_.at(static_cast<int32_t>(type)).data();
	}

	int32_t GetVertexShaderSize(MaterialShaderType type) const override
	{
		return static_cast<int32_t>(vertexShaders_.at(static_cast<int32_t>(type)).size());
	}

	const uint8_t* GetPixelShaderData(MaterialShaderType type) const override
	{
		return pixelShaders_.at(static_cast<int32_t>(type)).data();
	}

	int32_t GetPixelShaderSize(MaterialShaderType type) const override
	{
		return static_cast<int32_t>(pixelShaders_.at(static_cast<int32_t>(type)).size());
	}

	int AddRef() override
	{
		return ReferenceObject::AddRef();
	}

	int Release() override
	{
		return ReferenceObject::Release();
	}

	int GetRef() override
	{
		return ReferenceObject::GetRef();
	}
};

std::vector<uint8_t> CompileToWebGPUShaderBinary(const std::string& wgsl, LLGI::ShaderStageType stage)
{
	std::vector<uint8_t> ret;
	LLGI::CompilerWebGPU compiler;
	LLGI::CompilerResult result;
	compiler.Compile(result, wgsl.c_str(), stage);
	if (result.Binary.empty())
	{
		std::cout << "WebGPU Material WGSL Package Error" << std::endl;
		std::cout << result.Message << std::endl;
		std::cout << wgsl << std::endl;
		return ret;
	}

	Serialize(ret, result);
	return ret;
}
} // namespace

CompiledMaterialBinary* MaterialCompilerWebGPU::Compile(MaterialFile* material, int32_t maximumUniformCount, int32_t maximumTextureCount)
{
	auto binary = new CompiledMaterialBinaryWebGPU();

	auto saveBinary = [material, binary, maximumUniformCount, maximumTextureCount](MaterialShaderType type)
	{
		WGSL::ShaderGenerator generator;
		const auto shader = generator.GenerateShader(material, type, maximumUniformCount, maximumTextureCount, WebGPUModelRendererInstanceCount);
		binary->SetVertexShaderData(type, CompileToWebGPUShaderBinary(shader.CodeVS, LLGI::ShaderStageType::Vertex));
		binary->SetPixelShaderData(type, CompileToWebGPUShaderBinary(shader.CodePS, LLGI::ShaderStageType::Pixel));
	};

	if (material->GetHasRefraction())
	{
		saveBinary(MaterialShaderType::Refraction);
		saveBinary(MaterialShaderType::RefractionModel);
	}

	saveBinary(MaterialShaderType::Standard);
	saveBinary(MaterialShaderType::Model);

	return binary;
}

CompiledMaterialBinary* MaterialCompilerWebGPU::Compile(MaterialFile* material)
{
	return Compile(material, Effekseer::UserUniformSlotMax, Effekseer::UserTextureSlotMax);
}

} // namespace Effekseer

#ifdef __SHARED_OBJECT__

extern "C"
{

#ifdef _WIN32
#define EFK_EXPORT __declspec(dllexport)
#else
#define EFK_EXPORT
#endif

EFK_EXPORT Effekseer::MaterialCompiler* EFK_STDCALL CreateCompiler()
{
	return new Effekseer::MaterialCompilerWebGPU();
}
}
#endif
