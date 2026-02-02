#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include <string>

namespace Enjin {
namespace Gameplay {

class ENJIN_API HUDSystem {
public:
    void Update(ECS::World* world, const Renderer::Camera* gameCamera,
                f32 viewportX, f32 viewportY, f32 viewportW, f32 viewportH);
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    void DrawHealthBar(f32 anchorX, f32 anchorY, f32 width, f32 height,
                       f32 fillPct, const Math::Vector3& fillColor, const Math::Vector3& bgColor,
                       f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void DrawResourceBar(f32 anchorX, f32 anchorY, f32 width, f32 height,
                         f32 fillPct, const Math::Vector3& fillColor, const Math::Vector3& bgColor,
                         const std::string& label, f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void DrawLabel(const std::string& text, f32 anchorX, f32 anchorY,
                   const Math::Vector3& textColor, f32 fontSize,
                   f32 vpX, f32 vpY, f32 vpW, f32 vpH);
    void DrawCrosshair(f32 vpX, f32 vpY, f32 vpW, f32 vpH);

    bool m_Enabled = false;
};

} // namespace Gameplay
} // namespace Enjin
