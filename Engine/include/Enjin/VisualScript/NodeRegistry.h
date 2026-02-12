#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace VisualScript {

// ============================================================================
// NODE REGISTRY
// ============================================================================

// Singleton registry of all available node types
class ENJIN_API NodeRegistry {
public:
    // Get singleton instance
    static NodeRegistry& Instance();

    // Registration
    void RegisterNode(const NodeDefinition& def);
    void RegisterBuiltinNodes();

    // Lookup
    const NodeDefinition* FindNode(const std::string& typeId) const;
    std::vector<const NodeDefinition*> GetNodesByCategory(NodeCategory category) const;
    std::vector<const NodeDefinition*> GetAllNodes() const;

    // Search nodes by name/keywords (for add node menu)
    std::vector<const NodeDefinition*> SearchNodes(const std::string& query) const;

    // Get categories with at least one node
    std::vector<NodeCategory> GetActiveCategories() const;

private:
    NodeRegistry();
    ~NodeRegistry() = default;
    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    std::unordered_map<std::string, NodeDefinition> m_Nodes;
    bool m_Initialized = false;
};

// ============================================================================
// BUILT-IN NODE TYPE IDs
// ============================================================================

namespace NodeTypes {

// Events
constexpr const char* OnStart           = "Event_OnStart";
constexpr const char* OnUpdate          = "Event_OnUpdate";
constexpr const char* OnCollision       = "Event_OnCollision";
constexpr const char* CustomEvent       = "Event_Custom";
constexpr const char* OnCollisionEnter  = "Event_OnCollisionEnter";
constexpr const char* OnCollisionExit   = "Event_OnCollisionExit";
constexpr const char* OnTriggerEnter    = "Event_OnTriggerEnter";
constexpr const char* OnTriggerExit     = "Event_OnTriggerExit";

// Flow Control
constexpr const char* Branch        = "Flow_Branch";
constexpr const char* Sequence      = "Flow_Sequence";
constexpr const char* ForLoop       = "Flow_ForLoop";
constexpr const char* WhileLoop     = "Flow_WhileLoop";
constexpr const char* DoOnce        = "Flow_DoOnce";
constexpr const char* Gate          = "Flow_Gate";
constexpr const char* FlipFlop      = "Flow_FlipFlop";
constexpr const char* Delay         = "Flow_Delay";

// Variables
constexpr const char* GetVariable   = "Var_Get";
constexpr const char* SetVariable   = "Var_Set";
constexpr const char* GetSelf       = "Var_GetSelf";

// Math (Pure)
constexpr const char* Add           = "Math_Add";
constexpr const char* Subtract      = "Math_Subtract";
constexpr const char* Multiply      = "Math_Multiply";
constexpr const char* Divide        = "Math_Divide";
constexpr const char* Modulo        = "Math_Modulo";
constexpr const char* Power         = "Math_Power";
constexpr const char* Sqrt          = "Math_Sqrt";
constexpr const char* Negate        = "Math_Negate";
constexpr const char* Abs           = "Math_Abs";
constexpr const char* Min           = "Math_Min";
constexpr const char* Max           = "Math_Max";
constexpr const char* Clamp         = "Math_Clamp";
constexpr const char* Lerp          = "Math_Lerp";
constexpr const char* Floor         = "Math_Floor";
constexpr const char* Ceil          = "Math_Ceil";
constexpr const char* Round         = "Math_Round";
constexpr const char* Sin           = "Math_Sin";
constexpr const char* Cos           = "Math_Cos";
constexpr const char* Tan           = "Math_Tan";
constexpr const char* Atan2         = "Math_Atan2";
constexpr const char* RandomFloat   = "Math_RandomFloat";
constexpr const char* RandomInt     = "Math_RandomInt";

// Logic (Pure)
constexpr const char* And           = "Logic_And";
constexpr const char* Or            = "Logic_Or";
constexpr const char* Not           = "Logic_Not";
constexpr const char* Nand          = "Logic_Nand";
constexpr const char* Xor           = "Logic_Xor";
constexpr const char* Equal         = "Logic_Equal";
constexpr const char* NotEqual      = "Logic_NotEqual";
constexpr const char* Greater       = "Logic_Greater";
constexpr const char* GreaterEqual  = "Logic_GreaterEqual";
constexpr const char* Less          = "Logic_Less";
constexpr const char* LessEqual     = "Logic_LessEqual";

// Vector (Pure)
constexpr const char* MakeVector3     = "Vector_Make3";
constexpr const char* BreakVector3    = "Vector_Break3";
constexpr const char* VectorLength    = "Vector_Length";
constexpr const char* Normalize       = "Vector_Normalize";
constexpr const char* DotProduct      = "Vector_Dot";
constexpr const char* CrossProduct    = "Vector_Cross";
constexpr const char* Distance        = "Vector_Distance";
constexpr const char* LerpVector      = "Vector_Lerp";

// Transform
constexpr const char* GetPosition   = "Transform_GetPosition";
constexpr const char* SetPosition   = "Transform_SetPosition";
constexpr const char* GetRotation   = "Transform_GetRotation";
constexpr const char* SetRotation   = "Transform_SetRotation";
constexpr const char* GetScale      = "Transform_GetScale";
constexpr const char* SetScale      = "Transform_SetScale";
constexpr const char* Translate     = "Transform_Translate";
constexpr const char* Rotate        = "Transform_Rotate";
constexpr const char* LookAt        = "Transform_LookAt";

// Entity
constexpr const char* FindEntity    = "Entity_Find";
constexpr const char* DestroyEntity = "Entity_Destroy";
constexpr const char* SpawnEntity   = "Entity_Spawn";
constexpr const char* IsValid       = "Entity_IsValid";
constexpr const char* GetName       = "Entity_GetName";

// Physics
constexpr const char* Raycast       = "Physics_Raycast";
constexpr const char* SphereCheck   = "Physics_SphereCheck";
constexpr const char* BoxCheck      = "Physics_BoxCheck";
constexpr const char* AddForce      = "Physics_AddForce";
constexpr const char* AddImpulse    = "Physics_AddImpulse";
constexpr const char* SetVelocity   = "Physics_SetVelocity";
constexpr const char* GetVelocity   = "Physics_GetVelocity";
constexpr const char* SetGravityScale = "Physics_SetGravityScale";

// Component Access
constexpr const char* GetHealth     = "Component_GetHealth";
constexpr const char* SetHealth     = "Component_SetHealth";
constexpr const char* Damage        = "Component_Damage";
constexpr const char* HasComponent  = "Component_Has";

// Audio
constexpr const char* AudioPlay           = "Audio_Play";
constexpr const char* AudioStop           = "Audio_Stop";
constexpr const char* AudioIsPlaying      = "Audio_IsPlaying";
constexpr const char* AudioSetVolume      = "Audio_SetVolume";
constexpr const char* WaitForAudioComplete = "Audio_WaitComplete";

// Animation
constexpr const char* AnimatorPlay             = "Animator_Play";
constexpr const char* AnimatorSetSpeed         = "Animator_SetSpeed";
constexpr const char* AnimatorGetSpeed         = "Animator_GetSpeed";
constexpr const char* WaitForAnimationComplete = "Animator_WaitComplete";

// Debug
constexpr const char* Print         = "Debug_Print";
constexpr const char* PrintWarning  = "Debug_PrintWarning";
constexpr const char* PrintError    = "Debug_PrintError";

// Functions (Subgraphs)
constexpr const char* FunctionEntry  = "Function_Entry";
constexpr const char* FunctionReturn = "Function_Return";
constexpr const char* FunctionCall   = "Function_Call";

// Script Interop
constexpr const char* ScriptCall     = "Script_Call";

// Data Assets
constexpr const char* DataAssetLoad      = "DataAsset_Load";
constexpr const char* DataAssetGetFloat  = "DataAsset_GetFloat";
constexpr const char* DataAssetGetString = "DataAsset_GetString";

// Save System
constexpr const char* SaveToSlot         = "Gameplay_SaveToSlot";
constexpr const char* LoadFromSlot       = "Gameplay_LoadFromSlot";
constexpr const char* DeleteSlot         = "Gameplay_DeleteSlot";
constexpr const char* Checkpoint         = "Gameplay_Checkpoint";
constexpr const char* MetaSetFloat       = "Meta_SetFloat";
constexpr const char* MetaGetFloat       = "Meta_GetFloat";

// Weather
constexpr const char* WeatherSet           = "Weather_Set";
constexpr const char* WeatherSetFog        = "Weather_SetFog";

// Quests
constexpr const char* QuestStart           = "Quest_Start";
constexpr const char* QuestComplete        = "Quest_CompleteObjective";
constexpr const char* QuestIsActive        = "Quest_IsActive";

// Cinematics
constexpr const char* CinematicPlay        = "Cinematic_Play";
constexpr const char* CinematicStop        = "Cinematic_Stop";

// Particles
constexpr const char* ParticlePlay         = "Particle_Play";
constexpr const char* ParticleStop         = "Particle_Stop";
constexpr const char* ParticleBurst        = "Particle_Burst";

// Destructible
constexpr const char* DestructibleDamage   = "Destructible_Damage";

// Prefab
constexpr const char* PrefabInstantiate    = "Prefab_Instantiate";

// UI
constexpr const char* UISetText            = "UI_SetText";
constexpr const char* UISetProgress        = "UI_SetProgress";
constexpr const char* UISetVisible         = "UI_SetElementVisible";

// Localization
constexpr const char* LocGetString         = "Loc_Get";

// Flower
constexpr const char* FlowerGetTension     = "Flower_GetTension";
constexpr const char* FlowerIsBroken       = "Flower_IsBroken";
constexpr const char* FlowerIsGrabbed      = "Flower_IsGrabbed";
constexpr const char* FlowerGetScore       = "Flower_GetScore";
constexpr const char* FlowerSetBreakForce  = "Flower_SetBreakForce";

// Physics 2D
constexpr const char* Raycast2D            = "Physics2D_Raycast";
constexpr const char* OverlapCircle2D      = "Physics2D_OverlapCircle";
constexpr const char* OverlapBox2D         = "Physics2D_OverlapBox";
constexpr const char* AddForce2D           = "Physics2D_AddForce";
constexpr const char* AddImpulse2D         = "Physics2D_AddImpulse";

// Networking
constexpr const char* NetHostGame          = "Net_HostGame";
constexpr const char* NetJoinGame          = "Net_JoinGame";
constexpr const char* NetDisconnect        = "Net_Disconnect";
constexpr const char* NetIsConnected       = "Net_IsConnected";
constexpr const char* NetGetPing           = "Net_GetPing";
constexpr const char* NetCallRPC           = "Net_CallRPC";

// UI Focus
constexpr const char* UISetFocus           = "UI_SetFocus";
constexpr const char* UIClearFocus         = "UI_ClearFocus";
constexpr const char* UIGetFocusedElement  = "UI_GetFocusedElement";

} // namespace NodeTypes

} // namespace VisualScript
} // namespace Enjin
