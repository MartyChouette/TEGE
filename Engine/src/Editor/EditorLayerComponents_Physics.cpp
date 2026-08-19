// EditorLayerComponents_Physics.cpp — Physics component inspector draw functions
// Split from EditorLayerComponents.cpp for faster incremental builds.
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorTheme.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Animation/RagdollSystem.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <algorithm>
#include "Enjin/Math/Math.h"
#include <stb_image.h>

namespace Enjin {
using namespace Editor;

void EditorLayer::DrawRigidbodyComponent(ECS::Entity entity) {
    bool rbOpen = ImGui::CollapsingHeader("[R] Rigidbody", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("RigidbodyCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::RigidbodyComponent>(entity, "rigidbody", "Rigidbody");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (rbOpen) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (!rb) return;

        // Body type
        const char* bodyTypes[] = { "Dynamic", "Kinematic", "Static" };
        int currentType = static_cast<int>(rb->bodyType);
        if (InspectorUndo::Combo(m_UndoRedo, "Body Type", &currentType, bodyTypes, 3)) {
            rb->bodyType = static_cast<ECS::RigidbodyComponent::BodyType>(currentType);
        }
        ImGui::SetItemTooltip("Dynamic: physics-driven | Kinematic: script-driven | Static: immovable");

        InspectorUndo::DragFloat(m_UndoRedo, "Mass", &rb->mass, 0.1f, 0.001f, 1000.0f);
        ImGui::SetItemTooltip("Mass in kg (affects forces and collisions)");
        InspectorUndo::DragFloat(m_UndoRedo, "Drag", &rb->drag, 0.01f, 0.0f, 10.0f);
        ImGui::SetItemTooltip("Linear damping (air resistance)");
        InspectorUndo::DragFloat(m_UndoRedo, "Angular Drag", &rb->angularDrag, 0.01f, 0.0f, 10.0f);
        ImGui::SetItemTooltip("Rotational damping (spin resistance)");

        InspectorUndo::Checkbox(m_UndoRedo, "Use Gravity", &rb->useGravity);
        ImGui::SetItemTooltip("Apply gravitational force to this body");
        if (rb->useGravity) {
            InspectorUndo::DragFloat(m_UndoRedo, "Gravity Scale", &rb->gravityScale, 0.1f, -10.0f, 10.0f);
            ImGui::SetItemTooltip("Gravity multiplier (negative = anti-gravity)");
        }

        if (ImGui::TreeNode("Constraints")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Freeze X", &rb->freezePositionX);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Freeze Y", &rb->freezePositionY);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Freeze Z", &rb->freezePositionZ);

            InspectorUndo::Checkbox(m_UndoRedo, "Freeze Rot X", &rb->freezeRotationX);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Freeze Rot Y", &rb->freezeRotationY);
            ImGui::SameLine();
            InspectorUndo::Checkbox(m_UndoRedo, "Freeze Rot Z", &rb->freezeRotationZ);
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Velocity: %.2f, %.2f, %.2f", rb->velocity.x, rb->velocity.y, rb->velocity.z);
        ImGui::Text("Grounded: %s | Sleeping: %s", rb->isGrounded ? "Yes" : "No", rb->isSleeping ? "Yes" : "No");
    }
}

// Mesh-local AABB scaled by the entity's scale. Collider sizes are WORLD SPACE
// (Jolt/Box2D never multiply by transform scale), so "Fit to Mesh" bakes the
// entity scale into the collider dims. Returns false when there is no mesh.
static bool FitAABBFromMesh(ECS::World* world, ECS::Entity entity,
                            Math::Vector3& outCenter, Math::Vector3& outExtents) {
    auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
    if (!mesh) return false;
    Math::Vector3 mn = mesh->cachedAABBMin, mx = mesh->cachedAABBMax;
    if (mn.x > mx.x || mn.y > mx.y || mn.z > mx.z) {   // cache dirty -> recompute
        if (mesh->vertices.empty()) return false;
        mn = mx = mesh->vertices[0].position;
        for (const auto& v : mesh->vertices) {
            mn.x = std::min(mn.x, v.position.x); mx.x = std::max(mx.x, v.position.x);
            mn.y = std::min(mn.y, v.position.y); mx.y = std::max(mx.y, v.position.y);
            mn.z = std::min(mn.z, v.position.z); mx.z = std::max(mx.z, v.position.z);
        }
    }
    Math::Vector3 s(1.0f, 1.0f, 1.0f);
    if (auto* t = world->GetComponent<ECS::TransformComponent>(entity)) s = t->scale;
    outCenter = Math::Vector3((mn.x + mx.x) * 0.5f * s.x,
                              (mn.y + mx.y) * 0.5f * s.y,
                              (mn.z + mx.z) * 0.5f * s.z);
    outExtents = Math::Vector3(std::max((mx.x - mn.x) * std::abs(s.x), 0.001f),
                               std::max((mx.y - mn.y) * std::abs(s.y), 0.001f),
                               std::max((mx.z - mn.z) * std::abs(s.z), 0.001f));
    return true;
}

void EditorLayer::DrawBoxColliderComponent(ECS::Entity entity) {
    bool boxOpen = ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BoxColliderCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::BoxColliderComponent>(entity, "boxCollider", "Box Collider");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (boxOpen) {
        auto* col = m_World->GetComponent<ECS::BoxColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Center", center,
                [col](f32 x, f32 y, f32 z) { col->center = Math::Vector3(x, y, z); },
                0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }
        ImGui::SetItemTooltip("Local offset from entity origin");

        f32 size[3] = { col->size.x, col->size.y, col->size.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Size", size,
                [col](f32 x, f32 y, f32 z) { col->size = Math::Vector3(x, y, z); },
                0.1f, 0.001f, 1000.0f)) {
            col->size = Math::Vector3(size[0], size[1], size[2]);
        }
        ImGui::SetItemTooltip("Collider dimensions (width, height, depth)");

        InspectorUndo::Checkbox(m_UndoRedo, "Is Trigger", &col->isTrigger);
        ImGui::SetItemTooltip("Trigger colliders detect overlap but don't block movement");

        if (ImGui::TreeNode("Physics Material")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Surface friction (0 = ice, 1 = rubber)");
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Restitution (0 = no bounce, 1 = full bounce)");
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(col->categoryBits, col->collisionMask);

        if (ImGui::Button("Fit to Mesh##Box")) {
            Math::Vector3 c, ext;
            if (FitAABBFromMesh(m_World, entity, c, ext)) {
                col->center = c;
                col->size = ext;
            }
        }
        ImGui::SetItemTooltip("Size the collider to the mesh bounds (entity scale baked in - collider sizes are world-space)");
    }
}

void EditorLayer::DrawBody2DComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Body 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Body2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<Physics::Body2DComponent>(entity, "body2D", "Body 2D");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* body = m_World->GetComponent<Physics::Body2DComponent>(entity);
        if (!body) return;

        // Shape type
        const char* shapeNames[] = { "Circle", "Box", "Polygon", "Capsule" };
        int shapeIdx = static_cast<int>(body->shapeType);
        if (ImGui::Combo("Shape", &shapeIdx, shapeNames, 4)) {
            body->shapeType = static_cast<Physics::Shape2DType>(shapeIdx);
        }

        // Shape-specific properties
        if (body->shapeType == Physics::Shape2DType::Circle) {
            InspectorUndo::DragFloat(m_UndoRedo, "Radius", &body->circle.radius, 0.05f, 0.01f, 100.0f);
            f32 off[2] = { body->circle.offset.x, body->circle.offset.y };
            if (ImGui::DragFloat2("Offset", off, 0.1f)) {
                body->circle.offset = Math::Vector2(off[0], off[1]);
            }
        } else if (body->shapeType == Physics::Shape2DType::Box) {
            f32 he[2] = { body->box.halfExtents.x, body->box.halfExtents.y };
            if (ImGui::DragFloat2("Half Extents", he, 0.05f, 0.01f, 100.0f)) {
                body->box.halfExtents = Math::Vector2(he[0], he[1]);
            }
            f32 off[2] = { body->box.offset.x, body->box.offset.y };
            if (ImGui::DragFloat2("Offset##BoxOff", off, 0.1f)) {
                body->box.offset = Math::Vector2(off[0], off[1]);
            }
            InspectorUndo::DragFloat(m_UndoRedo, "Rotation##BoxRot", &body->box.rotation, 0.01f, -3.15f, 3.15f);
        } else if (body->shapeType == Physics::Shape2DType::Capsule) {
            InspectorUndo::DragFloat(m_UndoRedo, "Radius##Cap", &body->capsule.radius, 0.05f, 0.01f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Height##Cap", &body->capsule.height, 0.05f, 0.01f, 100.0f);
            f32 off[2] = { body->capsule.offset.x, body->capsule.offset.y };
            if (ImGui::DragFloat2("Offset##CapOff", off, 0.1f)) {
                body->capsule.offset = Math::Vector2(off[0], off[1]);
            }
        }

        ImGui::Separator();

        // Body properties
        InspectorUndo::Checkbox(m_UndoRedo, "Static", &body->isStatic);
        InspectorUndo::Checkbox(m_UndoRedo, "Kinematic", &body->isKinematic);
        InspectorUndo::Checkbox(m_UndoRedo, "Sensor", &body->isSensor);
        InspectorUndo::Checkbox(m_UndoRedo, "Fixed Rotation", &body->fixedRotation);
        InspectorUndo::DragFloat(m_UndoRedo, "Gravity Scale", &body->gravityScale, 0.1f, -10.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Linear Damping", &body->linearDamping, 0.05f, 0.0f, 20.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Angular Damping", &body->angularDamping, 0.05f, 0.0f, 20.0f);

        if (ImGui::TreeNode("Physics Material")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction", &body->material.friction, 0.05f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Restitution", &body->material.restitution, 0.05f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Density", &body->material.density, 0.1f, 0.01f, 100.0f);
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(body->categoryBits, body->collisionMask);
    }
}

void EditorLayer::DrawJoint2DComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Joint 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("Joint2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<Physics::Joint2DComponent>(entity, "joint2D", "Joint 2D");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* joint = m_World->GetComponent<Physics::Joint2DComponent>(entity);
        if (!joint) return;

        // Joint type
        const char* typeNames[] = { "Revolute", "Prismatic", "Distance", "Rope", "Weld" };
        int typeIdx = static_cast<int>(joint->type);
        if (ImGui::Combo("Type", &typeIdx, typeNames, 5)) {
            joint->type = static_cast<Physics::Joint2DType>(typeIdx);
        }

        // Connected entity
        u64 connId = static_cast<u64>(joint->connectedEntity);
        if (ImGui::InputScalar("Connected Entity", ImGuiDataType_U64, &connId)) {
            joint->connectedEntity = static_cast<ECS::Entity>(connId);
        }
        // Show name if valid
        if (joint->connectedEntity != 0 && m_World->IsValid(joint->connectedEntity)) {
            auto* name = m_World->GetComponent<ECS::NameComponent>(joint->connectedEntity);
            if (name) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", name->name.c_str());
            }
        }

        // Anchors
        f32 ancA[2] = { joint->anchorA.x, joint->anchorA.y };
        if (ImGui::DragFloat2("Anchor A", ancA, 0.05f)) {
            joint->anchorA = Math::Vector2(ancA[0], ancA[1]);
        }
        f32 ancB[2] = { joint->anchorB.x, joint->anchorB.y };
        if (ImGui::DragFloat2("Anchor B", ancB, 0.05f)) {
            joint->anchorB = Math::Vector2(ancB[0], ancB[1]);
        }

        ImGui::Separator();

        // Type-specific properties
        if (joint->type == Physics::Joint2DType::Revolute) {
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Limit##Rev", &joint->enableLimit);
            if (joint->enableLimit) {
                InspectorUndo::DragFloat(m_UndoRedo, "Lower Angle", &joint->lowerAngle, 0.01f, -6.28f, 6.28f);
                InspectorUndo::DragFloat(m_UndoRedo, "Upper Angle", &joint->upperAngle, 0.01f, -6.28f, 6.28f);
            }
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Motor##Rev", &joint->enableMotor);
            if (joint->enableMotor) {
                InspectorUndo::DragFloat(m_UndoRedo, "Motor Speed", &joint->motorSpeed, 0.1f, -100.0f, 100.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Max Motor Torque", &joint->maxMotorTorque, 0.1f, 0.0f, 10000.0f);
            }
        } else if (joint->type == Physics::Joint2DType::Prismatic) {
            f32 ax[2] = { joint->axis.x, joint->axis.y };
            if (ImGui::DragFloat2("Axis", ax, 0.05f)) {
                joint->axis = Math::Vector2(ax[0], ax[1]);
            }
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Limit##Pris", &joint->enableLimit);
            if (joint->enableLimit) {
                InspectorUndo::DragFloat(m_UndoRedo, "Lower Translation", &joint->lowerTranslation, 0.05f, -100.0f, 100.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Upper Translation", &joint->upperTranslation, 0.05f, -100.0f, 100.0f);
            }
            InspectorUndo::Checkbox(m_UndoRedo, "Enable Motor##Pris", &joint->enableMotor);
            if (joint->enableMotor) {
                InspectorUndo::DragFloat(m_UndoRedo, "Motor Speed##Pris", &joint->motorSpeed, 0.1f, -100.0f, 100.0f);
                InspectorUndo::DragFloat(m_UndoRedo, "Max Motor Torque##Pris", &joint->maxMotorTorque, 0.1f, 0.0f, 10000.0f);
            }
        } else if (joint->type == Physics::Joint2DType::Distance || joint->type == Physics::Joint2DType::Rope) {
            InspectorUndo::DragFloat(m_UndoRedo, "Length", &joint->length, 0.05f, 0.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Length", &joint->minLength, 0.05f, 0.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Length", &joint->maxLength, 0.05f, 0.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Stiffness", &joint->stiffness, 0.1f, 0.0f, 10000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Damping", &joint->damping, 0.1f, 0.0f, 1000.0f);
        }
        // Weld has no extra parameters

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Collide Connected", &joint->collideConnected);
    }
}

void EditorLayer::DrawSphereColliderComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* col = m_World->GetComponent<ECS::SphereColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Center", center,
                [col](f32 x, f32 y, f32 z) { col->center = Math::Vector3(x, y, z); },
                0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }
        ImGui::SetItemTooltip("Local offset from entity origin");

        InspectorUndo::DragFloat(m_UndoRedo, "Radius", &col->radius, 0.05f, 0.001f, 1000.0f);
        ImGui::SetItemTooltip("Sphere radius in world units");
        InspectorUndo::Checkbox(m_UndoRedo, "Is Trigger", &col->isTrigger);
        ImGui::SetItemTooltip("Trigger colliders detect overlap but don't block movement");

        if (ImGui::TreeNode("Physics Material")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Surface friction (0 = ice, 1 = rubber)");
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Restitution (0 = no bounce, 1 = full bounce)");
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(col->categoryBits, col->collisionMask);

        if (ImGui::Button("Fit to Mesh##Sphere")) {
            Math::Vector3 c, ext;
            if (FitAABBFromMesh(m_World, entity, c, ext)) {
                col->center = c;
                col->radius = std::max(ext.x, std::max(ext.y, ext.z)) * 0.5f;
            }
        }
        ImGui::SetItemTooltip("Size the collider to the mesh bounds (radius = largest extent / 2)");

        if (ImGui::BeginPopupContextItem("SphereColliderContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::SphereColliderComponent>(entity, "sphereCollider", "Sphere Collider");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawCapsuleColliderComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Capsule Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* col = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity);
        if (!col) return;

        f32 center[3] = { col->center.x, col->center.y, col->center.z };
        if (InspectorUndo::DragFloat3(m_UndoRedo, "Center", center,
                [col](f32 x, f32 y, f32 z) { col->center = Math::Vector3(x, y, z); },
                0.1f)) {
            col->center = Math::Vector3(center[0], center[1], center[2]);
        }
        ImGui::SetItemTooltip("Local offset from entity origin");

        InspectorUndo::DragFloat(m_UndoRedo, "Radius", &col->radius, 0.05f, 0.001f, 100.0f);
        ImGui::SetItemTooltip("Capsule hemisphere radius");
        InspectorUndo::DragFloat(m_UndoRedo, "Height", &col->height, 0.1f, 0.001f, 100.0f);
        ImGui::SetItemTooltip("Cylinder section only (engine convention): total = height + 2 x radius");

        const char* directions[] = { "X", "Y", "Z" };
        int dir = static_cast<int>(col->direction);
        if (InspectorUndo::Combo(m_UndoRedo, "Direction", &dir, directions, 3)) {
            col->direction = static_cast<ECS::CapsuleColliderComponent::Direction>(dir);
        }
        ImGui::SetItemTooltip("Primary axis of the capsule");

        InspectorUndo::Checkbox(m_UndoRedo, "Is Trigger", &col->isTrigger);
        ImGui::SetItemTooltip("Trigger colliders detect overlap but don't block movement");

        if (ImGui::TreeNode("Physics Material##Capsule")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Surface friction (0 = ice, 1 = rubber)");
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Restitution (0 = no bounce, 1 = full bounce)");
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(col->categoryBits, col->collisionMask);

        if (ImGui::Button("Fit to Mesh##Capsule")) {
            Math::Vector3 c, ext;
            if (FitAABBFromMesh(m_World, entity, c, ext)) {
                col->center = c;
                // Axis extent along the capsule direction; radius from the other two.
                f32 axisExt = ext.y, r1 = ext.x, r2 = ext.z;
                if (col->direction == ECS::CapsuleColliderComponent::Direction::X) {
                    axisExt = ext.x; r1 = ext.y; r2 = ext.z;
                } else if (col->direction == ECS::CapsuleColliderComponent::Direction::Z) {
                    axisExt = ext.z; r1 = ext.x; r2 = ext.y;
                }
                col->radius = std::max(r1, r2) * 0.5f;
                // height = cylinder only (total = height + 2r), clamped so a
                // squat mesh degrades to a sphere-ish capsule instead of inverting
                col->height = std::max(axisExt - 2.0f * col->radius, 0.001f);
            }
        }
        ImGui::SetItemTooltip("Size the collider to the mesh bounds. Fixes hovering characters:\ncapsule total height = height + 2 x radius, so height is fit as the cylinder section only");

        if (ImGui::BeginPopupContextItem("CapsuleColliderContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::CapsuleColliderComponent>(entity, "capsuleCollider", "Capsule Collider");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawMeshColliderComponent(ECS::Entity entity) {
    bool meshOpen = ImGui::CollapsingHeader("Mesh Collider", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MeshColliderCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MeshColliderComponent>(entity, "meshCollider", "Mesh Collider");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (meshOpen) {
        auto* col = m_World->GetComponent<ECS::MeshColliderComponent>(entity);
        if (!col) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Convex", &col->convex);
        ImGui::SetItemTooltip("Convex hull (dynamic/static) or triangle mesh (static only)");

        InspectorUndo::Checkbox(m_UndoRedo, "Auto Generate", &col->autoGenerate);
        ImGui::SetItemTooltip("Automatically generate collision from MeshComponent on first use");

        InspectorUndo::Checkbox(m_UndoRedo, "Is Trigger", &col->isTrigger);
        ImGui::SetItemTooltip("Trigger colliders detect overlap but don't block movement");

        // Regenerate button
        if (ImGui::Button("Regenerate")) {
            auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
            if (mesh && mesh->IsValid()) {
                col->vertices.clear();
                col->vertices.reserve(mesh->vertices.size());
                for (const auto& v : mesh->vertices) {
                    col->vertices.push_back(v.position);
                }
                col->indices = mesh->indices;
                col->generated = true;
            } else {
                ENJIN_LOG_WARN(Editor, "Cannot regenerate mesh collider: entity has no valid MeshComponent");
            }
        }
        ImGui::SetItemTooltip("Rebuild collision geometry from current mesh vertices");

        // Status display
        if (col->generated) {
            ImGui::Text("Vertices: %zu", col->vertices.size());
            if (!col->convex) {
                ImGui::Text("Triangles: %zu", col->indices.size() / 3);
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Not generated yet");
        }

        if (ImGui::TreeNode("Physics Material##Mesh")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction", &col->friction, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Surface friction (0 = ice, 1 = rubber)");
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness", &col->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Restitution (0 = no bounce, 1 = full bounce)");
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(col->categoryBits, col->collisionMask);
    }
}

void EditorLayer::DrawCollisionFilteringUI(u32& categoryBits, u32& collisionMask) {
    if (ImGui::TreeNode("Collision Filtering")) {
        const auto& groupNames = m_SceneManager.GetCollisionGroupNames();

        // Determine how many groups to show: all named groups + 2 blank slots
        int visibleCount = 1; // Always show at least "Default"
        for (int i = 1; i < 32; ++i) {
            if (!groupNames[i].empty()) visibleCount = i + 1;
        }
        visibleCount = std::min(visibleCount + 2, 32); // Show 2 extra blank slots

        ImGui::Text("Category (belongs to):");
        for (int i = 0; i < visibleCount; ++i) {
            const char* label = groupNames[i].empty() ?
                nullptr : groupNames[i].c_str();
            if (!label) continue; // Skip unnamed groups in category list

            ImGui::PushID(i);
            bool belongs = (categoryBits & (1u << i)) != 0;
            if (ImGui::Checkbox(label, &belongs)) {
                if (belongs) categoryBits |= (1u << i);
                else         categoryBits &= ~(1u << i);
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Text("Collides with:");
        for (int i = 0; i < visibleCount; ++i) {
            const char* label = groupNames[i].empty() ?
                nullptr : groupNames[i].c_str();
            if (!label) continue;

            ImGui::PushID(1000 + i);
            bool collides = (collisionMask & (1u << i)) != 0;
            if (ImGui::Checkbox(label, &collides)) {
                if (collides) collisionMask |= (1u << i);
                else          collisionMask &= ~(1u << i);
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Category: 0x%08X  Mask: 0x%08X", categoryBits, collisionMask);

        ImGui::TreePop();
    }
}

void EditorLayer::DrawTriggerZoneComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Trigger Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* zone = m_World->GetComponent<ECS::TriggerZoneComponent>(entity);
        if (!zone) return;

        const char* shapes[] = { "Box", "Sphere" };
        int shape = static_cast<int>(zone->shape);
        if (InspectorUndo::Combo(m_UndoRedo, "Shape", &shape, shapes, 2)) {
            zone->shape = static_cast<ECS::TriggerZoneComponent::Shape>(shape);
        }

        if (zone->shape == ECS::TriggerZoneComponent::Shape::Box) {
            f32 size[3] = { zone->boxSize.x, zone->boxSize.y, zone->boxSize.z };
            if (InspectorUndo::DragFloat3(m_UndoRedo, "Box Size", size, [zone](f32 x, f32 y, f32 z) { zone->boxSize = Math::Vector3(x, y, z); }, 0.1f, 0.01f, 1000.0f)) {
                zone->boxSize = Math::Vector3(size[0], size[1], size[2]);
            }
        } else {
            InspectorUndo::DragFloat(m_UndoRedo, "Sphere Radius", &zone->sphereRadius, 0.1f, 0.01f, 1000.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Trigger Once", &zone->triggerOnce);
        if (zone->triggerOnce) {
            ImGui::SameLine();
            ImGui::Text("(%s)", zone->hasTriggered ? "Triggered" : "Not triggered");
        }

        ImGui::Text("Entities Inside: %zu", zone->entitiesInside.size());

        if (ImGui::BeginPopupContextItem("TriggerZoneContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::TriggerZoneComponent>(entity, "triggerZone", "Trigger Zone");
            }
            ImGui::EndPopup();
        }
    }
}

void EditorLayer::DrawPerFrameColliderComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Per-Frame Collider", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("PerFrameColliderCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::PerFrameColliderComponent>(entity, "perFrameCollider", "Per-Frame Collider");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* pfc = m_World->GetComponent<ECS::PerFrameColliderComponent>(entity);
        if (!pfc) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Auto Apply##PFC", &pfc->autoApply);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically update BoxCollider on animation frame change");
        }

        ImGui::Text("Frame Colliders: %zu", pfc->frameColliders.size());

        // Match frame count to animation if present
        auto* anim = m_World->GetComponent<ECS::AnimatedSprite2DComponent>(entity);
        if (anim && !anim->frames.empty() && pfc->frameColliders.size() != anim->frames.size()) {
            if (ImGui::Button("Match Animation Frames##PFC")) {
                pfc->frameColliders.resize(anim->frames.size());
            }
        }

        for (usize i = 0; i < pfc->frameColliders.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            auto& fc = pfc->frameColliders[i];
            bool frameOpen = ImGui::TreeNode("##pfcframe", "Frame %zu", i);
            if (frameOpen) {
                f32 offset[2] = { fc.offset.x, fc.offset.y };
                if (ImGui::DragFloat2("Offset##PFC", offset, 0.01f)) {
                    fc.offset = Math::Vector2(offset[0], offset[1]);
                }
                f32 size[2] = { fc.size.x, fc.size.y };
                if (ImGui::DragFloat2("Size##PFC", size, 0.01f, 0.0f, 100.0f)) {
                    fc.size = Math::Vector2(size[0], size[1]);
                }
                ImGui::Checkbox("Enabled##PFC", &fc.enabled);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Frame##PFC")) {
            pfc->frameColliders.push_back(ECS::PerFrameColliderComponent::FrameCollider{});
        }
    }
}

void EditorLayer::DrawPolygonCollider2DComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Polygon Collider 2D", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("PolygonCollider2DCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::PolygonCollider2DComponent>(entity, "polygonCollider2D", "Polygon Collider 2D");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* poly = m_World->GetComponent<ECS::PolygonCollider2DComponent>(entity);
        if (!poly) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Is Trigger##Poly2D", &poly->isTrigger);

        if (ImGui::TreeNode("Physics Material##Poly2D")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Friction##Poly2D", &poly->friction, 0.05f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Bounciness##Poly2D", &poly->bounciness, 0.05f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        DrawCollisionFilteringUI(poly->categoryBits, poly->collisionMask);

        ImGui::Text("Vertices: %zu", poly->vertices.size());
        for (usize i = 0; i < poly->vertices.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            f32 v[2] = { poly->vertices[i].x, poly->vertices[i].y };
            char label[32];
            snprintf(label, sizeof(label), "V%zu##Poly", i);
            if (ImGui::DragFloat2(label, v, 0.01f)) {
                poly->vertices[i] = Math::Vector2(v[0], v[1]);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X##PolyDel")) {
                poly->vertices.erase(poly->vertices.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Vertex##Poly")) {
            poly->vertices.push_back(Math::Vector2(0, 0));
        }

        // Auto-generate from sprite
        if (m_World->HasComponent<ECS::Sprite2DComponent>(entity)) {
            auto* sprite = m_World->GetComponent<ECS::Sprite2DComponent>(entity);
            if (sprite && !sprite->texturePath.empty()) {
                ImGui::SameLine();
                if (ImGui::Button("Trace Silhouette##Poly")) {
                    int w, h, channels;
                    u8* pixels = stbi_load(sprite->texturePath.c_str(), &w, &h, &channels, 4);
                    if (pixels) {
                        Math::Vector2 sprSize(sprite->size.x > 0 ? sprite->size.x : 1.0f,
                                              sprite->size.y > 0 ? sprite->size.y : 1.0f);
                        auto result = SpriteColliderGenerator::FitPolygonCollider(
                            pixels, (u32)w, (u32)h, sprSize, sprite->pivot);
                        poly->vertices = result.vertices;
                        stbi_image_free(pixels);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Auto-trace polygon from sprite alpha silhouette");
                }
            }
        }
    }
}

void EditorLayer::DrawDistanceJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Distance Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("DistanceJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::DistanceJointComponent>(entity, "distanceJoint", "Distance Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::DistanceJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##DistJoint", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##DistJoint", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##DistJoint")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##DistJoint", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##DistJoint", &j->anchorB.x, 0.01f);

        InspectorUndo::DragFloat(m_UndoRedo, "Rest Distance", &j->restDistance, 0.1f, 0.0f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Tolerance", &j->tolerance, 0.01f, 0.0f, 100.0f);
        InspectorUndo::SliderFloat(m_UndoRedo, "Stiffness", &j->stiffness, 0.0f, 1.0f);

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##DistJoint", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##DistJoint", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawHingeJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Hinge Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("HingeJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::HingeJointComponent>(entity, "hingeJoint", "Hinge Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::HingeJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##HingeJoint", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##HingeJoint", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##HingeJoint")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##HingeJoint", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##HingeJoint", &j->anchorB.x, 0.01f);
        ImGui::DragFloat3("Axis##HingeJoint", &j->axis.x, 0.01f);

        InspectorUndo::Checkbox(m_UndoRedo, "Use Limits##HingeJoint", &j->useLimits);
        if (j->useLimits) {
            InspectorUndo::DragFloat(m_UndoRedo, "Lower Limit (deg)##HingeJoint", &j->lowerLimit, 1.0f, -360.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Upper Limit (deg)##HingeJoint", &j->upperLimit, 1.0f, 0.0f, 360.0f);
        }
        ImGui::Text("Current Angle: %.1f deg", j->currentAngle);

        InspectorUndo::Checkbox(m_UndoRedo, "Use Motor##HingeJoint", &j->useMotor);
        if (j->useMotor) {
            InspectorUndo::DragFloat(m_UndoRedo, "Motor Speed (deg/s)##HingeJoint", &j->motorSpeed, 1.0f, -1000.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Motor Force##HingeJoint", &j->motorMaxForce, 1.0f, 0.0f, 100000.0f);
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##HingeJoint", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##HingeJoint", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawBallSocketJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Ball-Socket Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BallSocketJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::BallSocketJointComponent>(entity, "ballSocketJoint", "Ball-Socket Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::BallSocketJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##BallSocket", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##BallSocket", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##BallSocket")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##BallSocket", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##BallSocket", &j->anchorB.x, 0.01f);

        InspectorUndo::Checkbox(m_UndoRedo, "Use Cone Limit##BallSocket", &j->useConeLimit);
        if (j->useConeLimit) {
            InspectorUndo::DragFloat(m_UndoRedo, "Cone Angle Limit (deg)", &j->coneAngleLimit, 1.0f, 0.0f, 180.0f);
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Use Twist Limit##BallSocket", &j->useTwistLimit);
        if (j->useTwistLimit) {
            InspectorUndo::DragFloat(m_UndoRedo, "Twist Lower (deg)##BallSocket", &j->twistLowerLimit, 1.0f, -180.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Twist Upper (deg)##BallSocket", &j->twistUpperLimit, 1.0f, 0.0f, 180.0f);
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##BallSocket", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##BallSocket", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawSpringJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Spring Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SpringJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::SpringJointComponent>(entity, "springJoint", "Spring Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::SpringJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##SpringJoint", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##SpringJoint", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##SpringJoint")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##SpringJoint", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##SpringJoint", &j->anchorB.x, 0.01f);

        InspectorUndo::DragFloat(m_UndoRedo, "Rest Length", &j->restLength, 0.1f, 0.0f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Spring Constant (k)", &j->springConstant, 1.0f, 0.0f, 10000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Damping Coefficient", &j->dampingCoefficient, 0.1f, 0.0f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Min Distance##SpringJoint", &j->minDistance, 0.1f, 0.0f, 1000.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Distance##SpringJoint", &j->maxDistance, 0.1f, 0.0f, 10000.0f);

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##SpringJoint", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##SpringJoint", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawFixedJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Fixed Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("FixedJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::FixedJointComponent>(entity, "fixedJoint", "Fixed Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::FixedJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##FixedJoint", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##FixedJoint", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##FixedJoint")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##FixedJoint", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##FixedJoint", &j->anchorB.x, 0.01f);

        ImGui::DragFloat3("Relative Position", &j->relativePosition.x, 0.01f);
        ImGui::DragFloat3("Relative Rotation (deg)", &j->relativeRotation.x, 1.0f);

        InspectorUndo::Checkbox(m_UndoRedo, "Initialized", &j->initialized);

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##FixedJoint", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##FixedJoint", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawSliderJointComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Slider Joint", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SliderJointCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::SliderJointComponent>(entity, "sliderJoint", "Slider Joint");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* j = m_World->GetComponent<ECS::SliderJointComponent>(entity);
        if (!j) return;

        u64 eA = static_cast<u64>(j->entityA);
        u64 eB = static_cast<u64>(j->entityB);
        if (ImGui::InputScalar("Entity A##SliderJoint", ImGuiDataType_U64, &eA)) {
            j->entityA = static_cast<ECS::Entity>(eA);
        }
        if (ImGui::InputScalar("Entity B##SliderJoint", ImGuiDataType_U64, &eB)) {
            j->entityB = static_cast<ECS::Entity>(eB);
        }
        if ((j->entityA == 0 || j->entityB == 0) && m_SelectedEntities.size() == 2) {
            if (ImGui::SmallButton("Auto-assign from selection##SliderJoint")) {
                ECS::Entity other = ECS::INVALID_ENTITY;
                for (auto sel : m_SelectedEntities) { if (sel != entity) { other = sel; break; } }
                if (other != ECS::INVALID_ENTITY) { j->entityA = entity; j->entityB = other; }
            }
        }

        ImGui::DragFloat3("Anchor A##SliderJoint", &j->anchorA.x, 0.01f);
        ImGui::DragFloat3("Anchor B##SliderJoint", &j->anchorB.x, 0.01f);
        ImGui::DragFloat3("Slide Axis", &j->slideAxis.x, 0.01f);

        InspectorUndo::Checkbox(m_UndoRedo, "Use Limits##SliderJoint", &j->useLimits);
        if (j->useLimits) {
            InspectorUndo::DragFloat(m_UndoRedo, "Lower Limit##SliderJoint", &j->lowerLimit, 0.1f, -1000.0f, 0.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Upper Limit##SliderJoint", &j->upperLimit, 0.1f, 0.0f, 1000.0f);
        }
        ImGui::Text("Current Displacement: %.3f", j->currentDisplacement);

        InspectorUndo::Checkbox(m_UndoRedo, "Use Motor##SliderJoint", &j->useMotor);
        if (j->useMotor) {
            InspectorUndo::DragFloat(m_UndoRedo, "Motor Speed##SliderJoint", &j->motorSpeed, 0.1f, -1000.0f, 1000.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Motor Force##SliderJoint", &j->motorMaxForce, 1.0f, 0.0f, 100000.0f);
        }

        ImGui::Separator();
        InspectorUndo::Checkbox(m_UndoRedo, "Breakable##SliderJoint", &j->breakable);
        if (j->breakable) {
            InspectorUndo::DragFloat(m_UndoRedo, "Break Force##SliderJoint", &j->breakForce, 1.0f, 0.0f, 100000.0f);
        }
        ImGui::Text("Current Stress: %.2f", j->currentStress);
    }
}

void EditorLayer::DrawRagdollComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("Ragdoll", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("RagdollCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::RagdollComponent>(entity, "ragdoll", "Ragdoll");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* r = m_World->GetComponent<ECS::RagdollComponent>(entity);
        if (!r) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##Ragdoll", &r->enabled);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto Activate On Death", &r->autoActivateOnDeath);
        ImGui::SetItemTooltip("Automatically activate ragdoll when HealthComponent reaches 0");

        if (ImGui::TreeNode("Transition")) {
            InspectorUndo::SliderFloat(m_UndoRedo, "Blend Weight", &r->blendWeight, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Blend Speed", &r->blendSpeed, 0.1f, 0.0f, 50.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Blend Time (s)", &r->blendTime, 0.01f, 0.0f, 5.0f);
            ImGui::SetItemTooltip("Duration of animation-to-ragdoll transition");
            ImGui::Text("Blend Progress: %.2f", r->blendProgress);
            ImGui::TreePop();
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Gravity Scale##Ragdoll", &r->gravityScale, 0.1f, -10.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Linear Damping##Ragdoll", &r->linearDamping, 0.01f, 0.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Angular Damping##Ragdoll", &r->angularDamping, 0.01f, 0.0f, 10.0f);

        if (ImGui::TreeNode("Settle Settings")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Auto Disable After Settle", &r->autoDisableAfterSettle);
            InspectorUndo::DragFloat(m_UndoRedo, "Settle Threshold", &r->settleThreshold, 0.001f, 0.0f, 1.0f, "%.4f");
            InspectorUndo::DragFloat(m_UndoRedo, "Settle Time (s)", &r->settleTime, 0.1f, 0.0f, 30.0f);
            ImGui::Text("Settle Timer: %.2f s", r->settleTimer);
            ImGui::TreePop();
        }

        // Generate from Skeleton button
        bool hasSkeleton = m_World->HasComponent<ECS::SkeletonComponent>(entity);
        if (!hasSkeleton) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Generate from Skeleton")) {
            Animation::RagdollSystem::GenerateFromSkeleton(m_World, entity);
        }
        if (!hasSkeleton) {
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Add a SkeletonComponent first");
        } else {
            ImGui::SetItemTooltip("Auto-generate bone joints from the entity's skeleton");
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear All Joints")) {
            r->boneJoints.clear();
        }

        ImGui::Text("Bone Joints: %u", static_cast<u32>(r->boneJoints.size()));

        if (ImGui::TreeNode("Bone Joints")) {
            for (usize i = 0; i < r->boneJoints.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& bj = r->boneJoints[i];
                if (ImGui::TreeNode("BoneJoint", "%s (idx %d, %.1f kg)", bj.boneName.empty() ? "(unnamed)" : bj.boneName.c_str(), bj.boneIndex, bj.mass)) {
                    char nameBuf[128];
                    strncpy(nameBuf, bj.boneName.c_str(), sizeof(nameBuf) - 1);
                    nameBuf[sizeof(nameBuf) - 1] = '\0';
                    if (ImGui::InputText("Bone Name", nameBuf, sizeof(nameBuf))) {
                        bj.boneName = nameBuf;
                    }
                    ImGui::InputInt("Bone Index", &bj.boneIndex);

                    const char* jointTypes[] = { "Distance", "Hinge", "BallSocket", "Spring", "Fixed", "Slider" };
                    int jt = static_cast<int>(bj.jointType);
                    if (ImGui::Combo("Joint Type##BJ", &jt, jointTypes, 6)) {
                        bj.jointType = static_cast<ECS::JointType>(jt);
                    }

                    u64 je = static_cast<u64>(bj.jointEntity);
                    if (ImGui::InputScalar("Joint Entity##BJ", ImGuiDataType_U64, &je)) {
                        bj.jointEntity = static_cast<ECS::Entity>(je);
                    }

                    ImGui::DragFloat("Mass##BJ", &bj.mass, 0.1f, 0.001f, 1000.0f);
                    ImGui::DragFloat("Collider Radius##BJ", &bj.colliderRadius, 0.01f, 0.001f, 10.0f);
                    ImGui::DragFloat3("Collider Size##BJ", &bj.colliderSize.x, 0.01f, 0.001f, 10.0f);
                    ImGui::SetItemTooltip("Capsule: (radius, halfHeight, 0)");
                    ImGui::DragFloat("Cone Angle Limit##BJ", &bj.coneAngleLimit, 1.0f, 0.0f, 180.0f);
                    ImGui::DragFloat("Twist Limit##BJ", &bj.twistLimit, 1.0f, 0.0f, 180.0f);

                    if (ImGui::Button("Remove##BJ")) {
                        r->boneJoints.erase(r->boneJoints.begin() + i);
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Bone Joint")) {
                r->boneJoints.push_back(ECS::RagdollComponent::BoneJoint{});
            }
            ImGui::TreePop();
        }
    }
}

// ============================================================================
// Animation Recorder Component
// ============================================================================


} // namespace Enjin
