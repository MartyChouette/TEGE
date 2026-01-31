#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

#include <vector>

namespace Enjin::GUI {

    enum class MinimapIcon : u8 {
        Player,
        Enemy,
        Ally,
        Objective,
        Waypoint,
        Custom
    };

    struct MinimapMarker {
        Math::Vector3 worldPosition;
        MinimapIcon icon = MinimapIcon::Custom;
        Math::Vector3 color = Math::Vector3(1, 1, 1);
        f32 size = 4.0f;
        bool pulse = false;
        ECS::Entity entity = 0;
    };

    struct MinimapConfig {
        bool enabled = true;
        f32 size = 200.0f;
        f32 worldRadius = 50.0f;
        f32 posX = 1.0f;
        f32 posY = 0.0f;
        f32 opacity = 0.8f;
        bool rotate = true;
        bool circular = true;
        Math::Vector3 bgColor = Math::Vector3(0.1f, 0.12f, 0.15f);
        Math::Vector3 borderColor = Math::Vector3(0.4f, 0.4f, 0.5f);
    };

    class ENJIN_API MinimapRenderer {
    public:
        void Render(f32 screenW, f32 screenH);

        void SetPlayerPosition(Math::Vector3 pos);
        void SetPlayerRotation(f32 yawDegrees);

        void ClearMarkers();
        void AddMarker(const MinimapMarker& marker);

        MinimapConfig& GetConfig();

    private:
        MinimapConfig m_Config;
        Math::Vector3 m_PlayerPos;
        f32 m_PlayerYaw = 0;
        std::vector<MinimapMarker> m_Markers;
    };

} // namespace Enjin::GUI
