#include "Enjin/Effects/EffekseerSystem.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/EffekseerEffect.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/AssetFS.h"

#include <Effekseer.h>

#if ENJIN_RENDERER_WEBGPU
#include <webgpu/webgpu_cpp.h>
#include <EffekseerRendererWebGPU.h>
#include <EffekseerRendererLLGI/EffekseerRendererLLGI.RendererImplemented.h>
#include <LLGI.CommandList.h>
#else
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanSwapchain.h"
#include <EffekseerRendererVulkan.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Effects {

namespace {

// Effekseer counts time in frames at this rate. Update() takes "how many
// frames passed", so a real delta time is multiplied by it.
constexpr f32 EFFEKSEER_FRAMES_PER_SECOND = 60.0f;

// Instances alive at once across every effect in the scene. Effekseer
// allocates for this up front and silently drops what does not fit.
constexpr i32 MAX_INSTANCES = 8000;

// Sprites (quads) one renderer can batch per draw.
constexpr i32 MAX_SPRITES = 8000;

// Frames a GPU resource is kept alive after its last use, so a frame still in
// flight never reads a freed buffer.
constexpr i32 SWAP_BUFFER_COUNT = 3;

std::string ToUtf8(const char16_t* s) {
    if (!s) return {};
    size_t len = 0;
    while (s[len]) ++len;
    std::string out(len * 4 + 1, '\0');
    const int32_t n = Effekseer::ConvertUtf16ToUtf8(out.data(), static_cast<int32_t>(out.size()), s);
    out.resize(n > 0 ? std::strlen(out.c_str()) : 0);
    return out;
}

std::u16string ToUtf16(const std::string& s) {
    std::u16string out(s.size() + 1, u'\0');
    Effekseer::ConvertUtf8ToUtf16(out.data(), static_cast<int32_t>(out.size()), s.c_str());
    size_t len = 0;
    while (len < out.size() && out[len]) ++len;
    out.resize(len);
    return out;
}

// The folder an effect file sits in, with a trailing slash. Effekseer joins
// it with the relative paths written inside the effect.
std::string DirectoryOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

// Effekseer reads every file an effect names (textures, models, materials,
// curves) through this. Going through AssetFS is what makes an effect work in
// a packed build and on web, where nothing is loose on disk.
class AssetFSReader final : public Effekseer::FileReader {
public:
    // Takes the bytes out of the caller's vector. An lvalue reference because
    // Effekseer::MakeRefPtr does not forward rvalues.
    explicit AssetFSReader(std::vector<u8>& bytes) { m_Bytes.swap(bytes); }
    size_t Read(void* buffer, size_t size) override {
        const size_t left = m_Bytes.size() - m_Pos;
        const size_t n = std::min(size, left);
        if (n > 0) std::memcpy(buffer, m_Bytes.data() + m_Pos, n);
        m_Pos += n;
        return n;
    }
    void Seek(int position) override {
        m_Pos = static_cast<size_t>(std::clamp<i64>(position, 0, static_cast<i64>(m_Bytes.size())));
    }
    int GetPosition() const override { return static_cast<int>(m_Pos); }
    size_t GetLength() const override { return m_Bytes.size(); }
private:
    std::vector<u8> m_Bytes;
    size_t m_Pos = 0;
};

class AssetFSInterface final : public Effekseer::FileInterface {
public:
    Effekseer::FileReaderRef OpenRead(const char16_t* path) override {
        const std::string utf8 = ToUtf8(path);
        std::vector<u8> bytes;
        if (!Platform::AssetFS::ReadBytes(utf8, bytes)) {
            ENJIN_LOG_WARN(Renderer, "Effekseer: could not read '%s'", utf8.c_str());
            return nullptr;
        }
        return Effekseer::MakeRefPtr<AssetFSReader>(bytes);
    }
    Effekseer::FileReaderRef TryOpenRead(const char16_t* path) override {
        const std::string utf8 = ToUtf8(path);
        std::vector<u8> bytes;
        if (!Platform::AssetFS::Exists(utf8) || !Platform::AssetFS::ReadBytes(utf8, bytes)) return nullptr;
        return Effekseer::MakeRefPtr<AssetFSReader>(bytes);
    }
    Effekseer::FileWriterRef OpenWrite(const char16_t*) override { return nullptr; }
};

// Engine matrices are column-major with the translation in m[12..14].
// Effekseer's are row-vector with the translation in the fourth row, which is
// the same sixteen floats in the same order.
Effekseer::Matrix44 ToEfk44(const Math::Matrix4& m) {
    Effekseer::Matrix44 out;
    std::memcpy(out.Values, m.m, sizeof(f32) * 16);
    return out;
}

Effekseer::Matrix43 ToEfk43(const Math::Matrix4& m) {
    Effekseer::Matrix43 out;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 3; ++c)
            out.Value[r][c] = m.m[r * 4 + c];
    return out;
}

// One Effekseer renderer and the per-renderer objects that go with it. A
// renderer builds its pipelines against one set of attachment formats, so
// each engine pass kind gets its own.
struct PassRenderer {
    EffekseerRenderer::RendererRef renderer;
    Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> memoryPool;
    Effekseer::RefPtr<EffekseerRenderer::CommandList> commandList;
    Effekseer::SpriteRendererRef sprite;
    Effekseer::RibbonRendererRef ribbon;
    Effekseer::RingRendererRef ring;
    Effekseer::TrackRendererRef track;
    Effekseer::ModelRendererRef model;

    bool Valid() const { return renderer != nullptr; }

    bool Finish() {
        if (renderer == nullptr) return false;
        memoryPool = EffekseerRenderer::CreateSingleFrameMemoryPool(renderer->GetGraphicsDevice());
        commandList = EffekseerRenderer::CreateCommandList(renderer->GetGraphicsDevice(), memoryPool);
        sprite = renderer->CreateSpriteRenderer();
        ribbon = renderer->CreateRibbonRenderer();
        ring = renderer->CreateRingRenderer();
        track = renderer->CreateTrackRenderer();
        model = renderer->CreateModelRenderer();
        return memoryPool != nullptr && commandList != nullptr;
    }

    void Reset() { *this = PassRenderer{}; }
};

} // namespace

struct EffekseerSystem::Impl {
    bool initialized = false;
    Effekseer::ManagerRef manager;
    Effekseer::RefPtr<AssetFSInterface> files;
    std::array<PassRenderer, static_cast<size_t>(Pass::Count)> passes;

    // Effect files by "path|magnification". An effect is immutable once
    // loaded, so every entity pointing at the same file shares one.
    std::unordered_map<std::string, Effekseer::EffectRef> effects;

    f32 timeSeconds = 0.0f;

    // Which live instance belongs to which entity. The component holds the
    // handle too, but a destroyed entity takes its component with it, and a
    // looping effect whose owner is gone would otherwise play forever.
    std::unordered_map<u64, Effekseer::Handle> owned;

#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanRenderer* vulkan = nullptr;
    VkCommandPool transferPool = VK_NULL_HANDLE;
#endif

    Effekseer::EffectRef LoadEffect(const std::string& path, f32 magnification, std::string& error) {
        const std::string key = path + "|" + std::to_string(magnification);
        auto it = effects.find(key);
        if (it != effects.end()) return it->second;

        std::vector<u8> bytes;
        if (!Platform::AssetFS::ReadBytes(path, bytes) || bytes.empty()) {
            error = "File not found: " + path;
            return nullptr;
        }
        const std::u16string materialPath = ToUtf16(DirectoryOf(path));
        Effekseer::EffectRef effect = Effekseer::Effect::Create(
            manager, bytes.data(), static_cast<int32_t>(bytes.size()), magnification,
            materialPath.empty() ? nullptr : materialPath.c_str());
        if (effect == nullptr) {
            error = "Not an Effekseer effect, or a version this runtime cannot read: " + path;
            return nullptr;
        }
        effects.emplace(key, effect);
        return effect;
    }

    void BindRenderer(const PassRenderer& p) {
        manager->SetSpriteRenderer(p.sprite);
        manager->SetRibbonRenderer(p.ribbon);
        manager->SetRingRenderer(p.ring);
        manager->SetTrackRenderer(p.track);
        manager->SetModelRenderer(p.model);
    }

    // The loaders upload through the device, not through a pass, so any one
    // renderer can create them for all.
    void InstallLoaders(const PassRenderer& p) {
        manager->SetTextureLoader(p.renderer->CreateTextureLoader(files));
        manager->SetModelLoader(p.renderer->CreateModelLoader(files));
        manager->SetMaterialLoader(p.renderer->CreateMaterialLoader(files));
        manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>(files));
    }
};

EffekseerSystem::EffekseerSystem() : m_Impl(std::make_unique<Impl>()) {}
EffekseerSystem::~EffekseerSystem() { Shutdown(); }

bool EffekseerSystem::IsInitialized() const { return m_Impl->initialized; }

#if ENJIN_RENDERER_WEBGPU

bool EffekseerSystem::Initialize(void* device) {
    if (m_Impl->initialized) return true;
    if (!device) return false;

    m_Impl->manager = Effekseer::Manager::Create(MAX_INSTANCES);
    m_Impl->files = Effekseer::MakeRefPtr<AssetFSInterface>();

    // The pipeline has to match the scene pass exactly or WebGPU drops every
    // draw in it (see "A new WebGPU pipeline drawing into the scene pass" in
    // CLAUDE.md). These are the scene pass's formats.
    EffekseerRendererWebGPU::RenderPassInformation info;
    info.DoesPresentToScreen = false;
    info.RenderTextureCount = 1;
    info.RenderTextureFormats[0] = wgpu::TextureFormat::RGBA16Float;
    info.DepthFormat = wgpu::TextureFormat::Depth24PlusStencil8;

    wgpu::Device wgpuDevice(static_cast<WGPUDevice>(device));   // adds a reference
    PassRenderer& p = m_Impl->passes[static_cast<size_t>(Pass::WebScene)];
    p.renderer = EffekseerRendererWebGPU::Create(wgpuDevice, info, MAX_SPRITES);
    if (!p.Finish()) {
        ENJIN_LOG_ERROR(Renderer, "Effekseer: could not create the WebGPU renderer");
        p.Reset();
        m_Impl->manager = nullptr;
        return false;
    }
    m_Impl->InstallLoaders(p);
    m_Impl->initialized = true;
    // The per-frame pools hand out nothing until their first NewFrame, and the
    // system is created mid-frame, on the first frame a scene has an effect.
    BeginFrame();
    ENJIN_LOG_INFO(Renderer, "Effekseer initialized (WebGPU)");
    return true;
}

#else

bool EffekseerSystem::Initialize(Renderer::VulkanRenderer* renderer) {
    if (m_Impl->initialized) return true;
    if (!renderer || !renderer->GetContext() || !renderer->GetSwapchain()) return false;

    Renderer::VulkanContext* ctx = renderer->GetContext();
    m_Impl->vulkan = renderer;

    // Effekseer uploads textures and models with short one-off command
    // buffers. They get a pool of their own so they never touch the pool the
    // frame's command buffer is being recorded from.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx->GetGraphicsQueueFamily();
    if (vkCreateCommandPool(ctx->GetDevice(), &poolInfo, nullptr, &m_Impl->transferPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Effekseer: could not create its transfer command pool");
        return false;
    }

    m_Impl->manager = Effekseer::Manager::Create(MAX_INSTANCES);
    m_Impl->files = Effekseer::MakeRefPtr<AssetFSInterface>();

    auto graphicsDevice = EffekseerRendererVulkan::CreateGraphicsDevice(
        ctx->GetPhysicalDevice(), ctx->GetDevice(), ctx->GetGraphicsQueue(),
        m_Impl->transferPool, SWAP_BUFFER_COUNT);

    // Main pass: the swapchain's two colour attachments (colour + velocity).
    // LLGI is patched to leave the second one untouched
    // (third_party/patches/llgi-mrt-colorwritemask.patch).
    EffekseerRendererVulkan::RenderPassInformation mainInfo;
    // false even though this pass does present. The flag only picks the
    // image layouts of the render pass LLGI builds for pipeline creation, and
    // with it set LLGI fills in attachment 0 alone, leaving the velocity
    // attachment with an undefined final layout (VUID 00843). Layouts play no
    // part in render pass compatibility, so the draw is the same either way.
    mainInfo.DoesPresentToScreen = false;
    mainInfo.RenderTextureCount = 2;
    mainInfo.RenderTextureFormats[0] = renderer->GetSwapchain()->GetImageFormat();
    mainInfo.RenderTextureFormats[1] = Renderer::VulkanSwapchain::VELOCITY_FORMAT;
    mainInfo.DepthFormat = renderer->GetSwapchain()->GetDepthFormat();

    // Offscreen RenderTarget: one colour attachment + depth. These two formats
    // are fixed in RenderTarget.cpp.
    EffekseerRendererVulkan::RenderPassInformation offscreenInfo;
    offscreenInfo.DoesPresentToScreen = false;
    offscreenInfo.RenderTextureCount = 1;
    offscreenInfo.RenderTextureFormats[0] = VK_FORMAT_B8G8R8A8_UNORM;
    offscreenInfo.DepthFormat = VK_FORMAT_D32_SFLOAT;

    PassRenderer& mainPass = m_Impl->passes[static_cast<size_t>(Pass::Main)];
    PassRenderer& offscreen = m_Impl->passes[static_cast<size_t>(Pass::Offscreen)];
    mainPass.renderer = EffekseerRendererVulkan::Create(graphicsDevice, mainInfo, MAX_SPRITES);
    offscreen.renderer = EffekseerRendererVulkan::Create(graphicsDevice, offscreenInfo, MAX_SPRITES);
    if (!mainPass.Finish() || !offscreen.Finish()) {
        ENJIN_LOG_ERROR(Renderer, "Effekseer: could not create the Vulkan renderers");
        Shutdown();
        return false;
    }
    m_Impl->InstallLoaders(offscreen);
    m_Impl->initialized = true;
    // The per-frame pools hand out nothing until their first NewFrame, and the
    // system is created mid-frame, on the first frame a scene has an effect.
    BeginFrame();
    ENJIN_LOG_INFO(Renderer, "Effekseer initialized (Vulkan)");
    return true;
}

#endif

void EffekseerSystem::Shutdown() {
    Impl& s = *m_Impl;
    if (s.manager != nullptr) s.manager->StopAllEffects();
    s.owned.clear();
    s.effects.clear();
    s.manager = nullptr;
    for (PassRenderer& p : s.passes) p.Reset();
    s.files = nullptr;
#if !ENJIN_RENDERER_WEBGPU
    if (s.transferPool != VK_NULL_HANDLE && s.vulkan && s.vulkan->GetContext()) {
        vkDestroyCommandPool(s.vulkan->GetContext()->GetDevice(), s.transferPool, nullptr);
    }
    s.transferPool = VK_NULL_HANDLE;
    s.vulkan = nullptr;
#endif
    s.initialized = false;
}

void EffekseerSystem::Update(ECS::World* world, f32 deltaTime) {
    Impl& s = *m_Impl;
    if (!s.initialized || !world) return;

    std::unordered_map<u64, Effekseer::Handle> stillOwned;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::EffekseerEffectComponent>()) {
        auto* c = world->GetComponent<ECS::EffekseerEffectComponent>(e);
        if (!c) continue;

        // A changed path or size means a different effect: the running
        // instance belongs to the old one and has to go.
        if (c->dirty) {
            c->dirty = false;
            c->loadError.clear();
            if (c->handle >= 0) s.manager->StopEffect(c->handle);
            c->handle = -1;
            c->playing = false;
            if (c->playOnStart && !c->effectPath.empty()) c->playRequested = true;
        }

        if (c->stopRequested) {
            c->stopRequested = false;
            if (c->handle >= 0) s.manager->StopEffect(c->handle);
            c->handle = -1;
            c->playing = false;
        }

        const bool alive = c->handle >= 0 && s.manager->Exists(c->handle);
        if (!alive && c->playing) {
            // Ran to its end. A looping effect starts over; any other is done.
            c->handle = -1;
            c->playing = false;
            if (c->loop) c->playRequested = true;
        }

        if (c->playRequested) {
            c->playRequested = false;
            if (c->handle >= 0) s.manager->StopEffect(c->handle);
            c->handle = -1;
            c->playing = false;
            if (!c->effectPath.empty()) {
                std::string error;
                Effekseer::EffectRef effect = s.LoadEffect(c->effectPath, c->magnification, error);
                if (effect == nullptr) {
                    if (c->loadError != error) {
                        ENJIN_LOG_WARN(Renderer, "Effekseer: %s", error.c_str());
                    }
                    c->loadError = error;
                } else {
                    c->loadError.clear();
                    c->handle = s.manager->Play(effect, 0.0f, 0.0f, 0.0f);
                    c->playing = c->handle >= 0;
                }
            }
        }

        if (c->handle >= 0) {
            s.manager->SetMatrix(c->handle, ToEfk43(ECS::ComputeWorldMatrix(world, e)));
            s.manager->SetSpeed(c->handle, c->speed);
            s.manager->SetShown(c->handle, c->visible);
            stillOwned[static_cast<u64>(e)] = c->handle;
        }
    }

    // Stop what belonged to an entity that no longer exists or no longer has
    // the component.
    for (const auto& [entity, handle] : s.owned) {
        auto it = stillOwned.find(entity);
        if (it == stillOwned.end() || it->second != handle) s.manager->StopEffect(handle);
    }
    s.owned.swap(stillOwned);

    s.timeSeconds += deltaTime;
    Effekseer::Manager::UpdateParameter update;
    update.DeltaFrame = deltaTime * EFFEKSEER_FRAMES_PER_SECOND;
    s.manager->Update(update);
}

void EffekseerSystem::StopAll(ECS::World* world) {
    Impl& s = *m_Impl;
    if (s.manager != nullptr) s.manager->StopAllEffects();
    s.owned.clear();
    if (!world) return;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::EffekseerEffectComponent>()) {
        if (auto* c = world->GetComponent<ECS::EffekseerEffectComponent>(e)) {
            c->handle = -1;
            c->playing = false;
            c->playRequested = false;
            c->stopRequested = false;
            c->dirty = true;   // the next Update starts playOnStart effects again
        }
    }
}

void EffekseerSystem::ReloadEffects(ECS::World* world) {
    StopAll(world);
    m_Impl->effects.clear();
}

void EffekseerSystem::BeginFrame() {
    Impl& s = *m_Impl;
    if (!s.initialized) return;
    for (PassRenderer& p : s.passes) {
        if (p.Valid()) p.memoryPool->NewFrame();
    }
}

void EffekseerSystem::Draw(Pass pass, void* nativeCommands, void* nativePass,
                           const Math::Matrix4& view, const Math::Matrix4& proj,
                           const Math::Vector3& cameraPosition) {
    Impl& s = *m_Impl;
    if (!s.initialized || pass >= Pass::Count) return;
    PassRenderer& p = s.passes[static_cast<size_t>(pass)];
    if (!p.Valid() || !nativeCommands) return;
    if (s.manager->GetTotalInstanceCount() <= 0) return;

#if ENJIN_RENDERER_WEBGPU
    if (!nativePass) return;
    // LLGI adopts both handles with wgpu::Acquire, which takes over a
    // reference it was never given, and drops it again when the command list
    // ends. Add the reference it is about to release.
    wgpuCommandEncoderAddRef(static_cast<WGPUCommandEncoder>(nativeCommands));
    wgpuRenderPassEncoderAddRef(static_cast<WGPURenderPassEncoder>(nativePass));
    auto* llgiList = static_cast<EffekseerRendererLLGI::CommandList*>(p.commandList.Get());
    llgiList->GetInternal()->BeginWithPlatform(nativeCommands);
    llgiList->GetInternal()->BeginRenderPassWithPlatformPtr(nativePass);
    // The web projection is the engine's own; no flip to undo.
    const Math::Matrix4& efkProj = proj;
#else
    (void)nativePass;
    EffekseerRendererVulkan::BeginCommandList(p.commandList, static_cast<VkCommandBuffer>(nativeCommands));
    // Matrix4::Perspective flips Y for Vulkan (m[5] negated). Effekseer's
    // Vulkan shaders flip Y themselves, so hand them the matrix un-flipped or
    // every effect draws upside down with its winding reversed.
    Math::Matrix4 efkProj = proj;
    efkProj.m[1] = -efkProj.m[1];
    efkProj.m[5] = -efkProj.m[5];
    efkProj.m[9] = -efkProj.m[9];
    efkProj.m[13] = -efkProj.m[13];
#endif

    s.BindRenderer(p);
    p.renderer->SetCommandList(p.commandList);
    p.renderer->SetTime(s.timeSeconds);
    p.renderer->SetProjectionMatrix(ToEfk44(efkProj));
    p.renderer->SetCameraMatrix(ToEfk44(view));

    Effekseer::Manager::LayerParameter layer;
    layer.ViewerPosition = Effekseer::Vector3D(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    s.manager->SetLayerParameter(0, layer);

    p.renderer->BeginRendering();
    Effekseer::Manager::DrawParameter draw;
    draw.ZNear = 0.0f;
    draw.ZFar = 1.0f;
    draw.ViewProjectionMatrix = p.renderer->GetCameraProjectionMatrix();
    s.manager->Draw(draw);
    p.renderer->EndRendering();
    p.renderer->SetCommandList(nullptr);

#if ENJIN_RENDERER_WEBGPU
    llgiList->GetInternal()->EndRenderPassWithPlatformPtr();
    llgiList->GetInternal()->EndWithPlatform();
#else
    EffekseerRendererVulkan::EndCommandList(p.commandList);
#endif
}

u32 EffekseerSystem::GetLiveInstanceCount() const {
    return m_Impl->manager != nullptr ? static_cast<u32>(std::max(0, m_Impl->manager->GetTotalInstanceCount())) : 0u;
}

u32 EffekseerSystem::GetLoadedEffectCount() const {
    return static_cast<u32>(m_Impl->effects.size());
}

} // namespace Effects
} // namespace Enjin
