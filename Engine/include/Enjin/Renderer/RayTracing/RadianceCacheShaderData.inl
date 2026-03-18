// Radiance Cache update compute — see Engine/shaders/radiance_cache_update.comp
// Placeholder SPIR-V: minimal compute shader that just returns.
// Regenerate from compiled radiance_cache_update.comp.spv using the embedding script.
static const uint32_t RADIANCE_CACHE_UPDATE_COMP_SPV[] = {
    // SPIR-V magic + version 1.5 + generator + bound + schema
    0x07230203, 0x00010500, 0x0008000B, 0x00000006, 0x00000000,
    // OpCapability Shader
    0x00020011, 0x00000001,
    // OpExtInstImport "GLSL.std.450"
    0x0006000B, 0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000,
    // OpMemoryModel Logical GLSL450
    0x0003000E, 0x00000000, 0x00000001,
    // OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
    0x0008000F, 0x00000005, 0x00000004, 0x6E69616D, 0x00000000, 0x00000005, 0x00000000, 0x00000000,
    // OpExecutionMode %main LocalSize 8 8 1
    0x00060010, 0x00000004, 0x00000011, 0x00000008, 0x00000008, 0x00000001,
    // OpName %main "main"
    0x00040005, 0x00000004, 0x6E69616D, 0x00000000,
    // Types
    0x00020013, 0x00000002, // OpTypeVoid
    0x00030021, 0x00000003, 0x00000002, // OpTypeFunction %void
    // %main = OpFunction %void None %func
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003,
    // OpLabel
    0x000200F8, 0x00000005,
    // OpReturn
    0x000100FD,
    // OpFunctionEnd
    0x00010038
};

// Radiance Cache read compute — see Engine/shaders/radiance_cache_read.comp
// Placeholder SPIR-V: minimal compute shader that just returns.
// Regenerate from compiled radiance_cache_read.comp.spv using the embedding script.
static const uint32_t RADIANCE_CACHE_READ_COMP_SPV[] = {
    // SPIR-V magic + version 1.5 + generator + bound + schema
    0x07230203, 0x00010500, 0x0008000B, 0x00000006, 0x00000000,
    // OpCapability Shader
    0x00020011, 0x00000001,
    // OpExtInstImport "GLSL.std.450"
    0x0006000B, 0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000,
    // OpMemoryModel Logical GLSL450
    0x0003000E, 0x00000000, 0x00000001,
    // OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
    0x0008000F, 0x00000005, 0x00000004, 0x6E69616D, 0x00000000, 0x00000005, 0x00000000, 0x00000000,
    // OpExecutionMode %main LocalSize 8 8 1
    0x00060010, 0x00000004, 0x00000011, 0x00000008, 0x00000008, 0x00000001,
    // OpName %main "main"
    0x00040005, 0x00000004, 0x6E69616D, 0x00000000,
    // Types
    0x00020013, 0x00000002, // OpTypeVoid
    0x00030021, 0x00000003, 0x00000002, // OpTypeFunction %void
    // %main = OpFunction %void None %func
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003,
    // OpLabel
    0x000200F8, 0x00000005,
    // OpReturn
    0x000100FD,
    // OpFunctionEnd
    0x00010038
};
