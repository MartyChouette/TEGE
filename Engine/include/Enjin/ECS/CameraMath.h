#pragma once
// Turning screen pixels into world points, and back.
//
// The engine held the projection matrix all along and never exposed this, so
// every consumer that needed it wrote its own: the editor's picker
// (ScenePicker::ScreenToRay), FlowerSystem::ScreenToWorldOnPlane and
// CollaborativeEditingUI::WorldToScreen each rolled their own, and a game
// script could not do it at all. One project reimplemented orthographic
// unprojection in AngelScript from the screen size plus a hand-copied
// orthoSize -- which means the camera's size now lives in two files, and
// changing it in the scene silently sends every click to the wrong place.
//
// A camera ENTITY is what a script and a system actually have, so that is what
// these take. The Renderer::Camera the renderer builds from that entity is an
// implementation detail and is built here the same way, in one place.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {

namespace Renderer { class Camera; }

namespace ECS {

class World;

// Fills `outCamera` from a camera entity's CameraComponent + TransformComponent,
// exactly as the render path does. False if either component is missing.
ENJIN_API bool BuildCameraFromEntity(World* world, Entity cameraEntity, f32 aspect,
                                     Renderer::Camera& outCamera);

// The same unprojection given a view-projection matrix directly.
//
// The entity forms below are wrappers over this. It exists separately because
// the editor's picker and the script raycast already hold a Renderer::Camera
// rather than an entity, and routing them through here is what lets screen
// picking work on WEB at all -- the previous path called into the Editor
// module, which the web build excludes, so it returned "nothing hit" always.
ENJIN_API bool ScreenToRayVP(const Math::Matrix4& viewProjection,
                             const Math::Vector2& screen,
                             f32 viewportWidth, f32 viewportHeight,
                             Math::Vector3& outOrigin, Math::Vector3& outDirection);

// A ray through a screen pixel, in world space.
//
// `screen` is in pixels with the origin at the TOP-LEFT, which is what every
// mouse API in the engine reports.
ENJIN_API bool ScreenToRay(World* world, Entity cameraEntity,
                           const Math::Vector2& screen,
                           f32 viewportWidth, f32 viewportHeight,
                           Math::Vector3& outOrigin, Math::Vector3& outDirection);

// Where that ray crosses a plane of constant Z. This is the 2D case: a board,
// a card field, anything laid out on z = planeZ.
//
// False when the ray runs parallel to the plane, which is the one case a
// caller must not treat as a position.
ENJIN_API bool ScreenToWorldOnPlane(World* world, Entity cameraEntity,
                                    const Math::Vector2& screen,
                                    f32 viewportWidth, f32 viewportHeight,
                                    f32 planeZ, Math::Vector3& outWorld);

// The inverse: a world point as a screen pixel, top-left origin.
//
// False when the point is BEHIND the camera, where the perspective divide
// flips the sign and would otherwise report a confident on-screen position for
// something nobody can see.
ENJIN_API bool WorldToScreen(World* world, Entity cameraEntity,
                             const Math::Vector3& worldPoint,
                             f32 viewportWidth, f32 viewportHeight,
                             Math::Vector2& outScreen);

} // namespace ECS
} // namespace Enjin
