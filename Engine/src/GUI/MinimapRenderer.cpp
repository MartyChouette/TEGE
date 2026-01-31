#include "Enjin/GUI/MinimapRenderer.h"

#include <imgui.h>
#include <cmath>

namespace Enjin::GUI {

    static constexpr f32 PI = 3.14159265358979323846f;

    static ImU32 Vec3ToColor(const Math::Vector3& c, f32 alpha) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, alpha));
    }

    static ImVec2 RotatePoint(ImVec2 point, f32 angleRad) {
        f32 cosA = std::cos(angleRad);
        f32 sinA = std::sin(angleRad);
        return ImVec2(
            point.x * cosA - point.y * sinA,
            point.x * sinA + point.y * cosA
        );
    }

    void MinimapRenderer::Render(f32 screenW, f32 screenH) {
        if (!m_Config.enabled) return;

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return;

        f32 halfSize = m_Config.size * 0.5f;
        f32 padding = 10.0f;

        // Compute the center of the minimap based on posX/posY anchors.
        // posX: 0 = left edge, 1 = right edge
        // posY: 0 = top edge, 1 = bottom edge
        f32 centerX = padding + halfSize + m_Config.posX * (screenW - m_Config.size - 2.0f * padding);
        f32 centerY = padding + halfSize + m_Config.posY * (screenH - m_Config.size - 2.0f * padding);
        ImVec2 center(centerX, centerY);

        f32 yawRad = m_Config.rotate ? (-m_PlayerYaw * PI / 180.0f) : 0.0f;
        f32 opacity = m_Config.opacity;

        // Draw background
        ImU32 bgCol = Vec3ToColor(m_Config.bgColor, opacity);
        ImU32 borderCol = Vec3ToColor(m_Config.borderColor, opacity);

        if (m_Config.circular) {
            drawList->AddCircleFilled(center, halfSize, bgCol, 64);
            drawList->AddCircle(center, halfSize, borderCol, 64, 2.0f);
        } else {
            ImVec2 topLeft(centerX - halfSize, centerY - halfSize);
            ImVec2 bottomRight(centerX + halfSize, centerY + halfSize);
            drawList->AddRectFilled(topLeft, bottomRight, bgCol);
            drawList->AddRect(topLeft, bottomRight, borderCol, 0.0f, 0, 2.0f);
        }

        // Helper: transform world position to minimap screen position
        // Returns false if the point is outside the minimap bounds
        auto WorldToMinimap = [&](const Math::Vector3& worldPos, ImVec2& outScreen) -> bool {
            f32 dx = worldPos.x - m_PlayerPos.x;
            f32 dz = worldPos.z - m_PlayerPos.z;

            // Scale from world to minimap space
            f32 scale = halfSize / m_Config.worldRadius;
            f32 mx = dx * scale;
            f32 my = -dz * scale; // Negate Z so forward is up on the minimap

            // Rotate if the map follows the player's facing direction
            if (m_Config.rotate) {
                ImVec2 rotated = RotatePoint(ImVec2(mx, my), yawRad);
                mx = rotated.x;
                my = rotated.y;
            }

            // Clamp to minimap bounds
            if (m_Config.circular) {
                f32 dist = std::sqrt(mx * mx + my * my);
                f32 limit = halfSize - 4.0f;
                if (dist > limit) {
                    mx = mx / dist * limit;
                    my = my / dist * limit;
                }
            } else {
                f32 limit = halfSize - 4.0f;
                mx = std::max(-limit, std::min(limit, mx));
                my = std::max(-limit, std::min(limit, my));
            }

            outScreen = ImVec2(centerX + mx, centerY + my);

            // Return true if the original point was within the world radius
            f32 worldDist = std::sqrt(dx * dx + dz * dz);
            return worldDist <= m_Config.worldRadius;
        };

        // Draw markers
        f64 time = ImGui::GetTime();

        for (const auto& marker : m_Markers) {
            ImVec2 screenPos;
            WorldToMinimap(marker.worldPosition, screenPos);

            f32 markerSize = marker.size;
            if (marker.pulse) {
                markerSize *= 1.0f + 0.3f * std::sin(static_cast<f32>(time) * 4.0f);
            }

            ImU32 markerCol = Vec3ToColor(marker.color, opacity);

            switch (marker.icon) {
                case MinimapIcon::Enemy: {
                    // Diamond shape for enemies
                    ImVec2 top(screenPos.x, screenPos.y - markerSize);
                    ImVec2 right(screenPos.x + markerSize, screenPos.y);
                    ImVec2 bottom(screenPos.x, screenPos.y + markerSize);
                    ImVec2 left(screenPos.x - markerSize, screenPos.y);
                    drawList->AddQuadFilled(top, right, bottom, left, markerCol);
                    break;
                }
                case MinimapIcon::Ally: {
                    // Circle for allies
                    drawList->AddCircleFilled(screenPos, markerSize, markerCol, 16);
                    break;
                }
                case MinimapIcon::Objective: {
                    // Star-like: filled circle with a brighter outline
                    drawList->AddCircleFilled(screenPos, markerSize, markerCol, 16);
                    ImU32 brightCol = Vec3ToColor(
                        Math::Vector3(
                            std::min(1.0f, marker.color.x + 0.3f),
                            std::min(1.0f, marker.color.y + 0.3f),
                            std::min(1.0f, marker.color.z + 0.3f)
                        ), opacity);
                    drawList->AddCircle(screenPos, markerSize + 2.0f, brightCol, 16, 1.5f);
                    break;
                }
                case MinimapIcon::Waypoint: {
                    // Upward-pointing triangle for waypoints
                    f32 h = markerSize * 1.2f;
                    ImVec2 p1(screenPos.x, screenPos.y - h);
                    ImVec2 p2(screenPos.x - markerSize, screenPos.y + markerSize * 0.6f);
                    ImVec2 p3(screenPos.x + markerSize, screenPos.y + markerSize * 0.6f);
                    drawList->AddTriangleFilled(p1, p2, p3, markerCol);
                    break;
                }
                case MinimapIcon::Custom:
                default: {
                    // Small square for custom markers
                    ImVec2 tl(screenPos.x - markerSize * 0.5f, screenPos.y - markerSize * 0.5f);
                    ImVec2 br(screenPos.x + markerSize * 0.5f, screenPos.y + markerSize * 0.5f);
                    drawList->AddRectFilled(tl, br, markerCol);
                    break;
                }
                case MinimapIcon::Player:
                    // Player icons drawn via markers use the same triangle as the main player indicator
                    break;
            }
        }

        // Draw player indicator at center: a triangle pointing in the yaw direction
        {
            f32 arrowLen = 8.0f;
            f32 arrowHalf = 5.0f;

            // The player arrow points "up" (negative Y in screen) and is rotated by yaw
            ImVec2 tip(0.0f, -arrowLen);
            ImVec2 left(-arrowHalf, arrowHalf);
            ImVec2 right(arrowHalf, arrowHalf);

            // Rotate all points by yaw (when rotate is on, the map rotates so the arrow stays up)
            f32 arrowAngle = m_Config.rotate ? 0.0f : (-m_PlayerYaw * PI / 180.0f);
            tip = RotatePoint(tip, arrowAngle);
            left = RotatePoint(left, arrowAngle);
            right = RotatePoint(right, arrowAngle);

            ImVec2 p1(center.x + tip.x, center.y + tip.y);
            ImVec2 p2(center.x + left.x, center.y + left.y);
            ImVec2 p3(center.x + right.x, center.y + right.y);

            ImU32 playerCol = Vec3ToColor(Math::Vector3(0.2f, 0.9f, 0.3f), opacity);
            drawList->AddTriangleFilled(p1, p2, p3, playerCol);

            // Thin border around the player arrow for visibility
            ImU32 playerBorder = Vec3ToColor(Math::Vector3(1.0f, 1.0f, 1.0f), opacity * 0.6f);
            drawList->AddTriangle(p1, p2, p3, playerBorder, 1.0f);
        }

        // Draw cardinal direction labels on the rim
        {
            f32 labelDist = halfSize - 12.0f;
            const char* labels[] = { "N", "E", "S", "W" };
            f32 angles[] = { 0.0f, PI * 0.5f, PI, PI * 1.5f };

            ImU32 textCol = Vec3ToColor(Math::Vector3(0.8f, 0.8f, 0.85f), opacity * 0.7f);

            for (i32 i = 0; i < 4; i++) {
                f32 angle = angles[i] + yawRad;
                f32 lx = std::sin(angle) * labelDist;
                f32 ly = -std::cos(angle) * labelDist;

                ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
                ImVec2 textPos(
                    centerX + lx - textSize.x * 0.5f,
                    centerY + ly - textSize.y * 0.5f
                );
                drawList->AddText(textPos, textCol, labels[i]);
            }
        }
    }

    void MinimapRenderer::SetPlayerPosition(Math::Vector3 pos) {
        m_PlayerPos = pos;
    }

    void MinimapRenderer::SetPlayerRotation(f32 yawDegrees) {
        m_PlayerYaw = yawDegrees;
    }

    void MinimapRenderer::ClearMarkers() {
        m_Markers.clear();
    }

    void MinimapRenderer::AddMarker(const MinimapMarker& marker) {
        m_Markers.push_back(marker);
    }

    MinimapConfig& MinimapRenderer::GetConfig() {
        return m_Config;
    }

} // namespace Enjin::GUI
