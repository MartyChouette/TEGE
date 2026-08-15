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

// Hierarchy
constexpr const char* EntitySetParent    = "Entity_SetParent";
constexpr const char* EntityRemoveParent = "Entity_RemoveParent";
constexpr const char* EntityGetParent    = "Entity_GetParent";
constexpr const char* EntityGetChildCount = "Entity_GetChildCount";
constexpr const char* EntityGetChild     = "Entity_GetChild";

// Tags
constexpr const char* EntityAddTag       = "Entity_AddTag";
constexpr const char* EntityRemoveTag    = "Entity_RemoveTag";
constexpr const char* EntityHasTag       = "Entity_HasTag";
constexpr const char* FindEntityByTag    = "Entity_FindByTag";

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
constexpr const char* AudioPlay              = "Audio_Play";
constexpr const char* AudioStop              = "Audio_Stop";
constexpr const char* AudioIsPlaying         = "Audio_IsPlaying";
constexpr const char* AudioSetVolume         = "Audio_SetVolume";
constexpr const char* AudioSetChannelVolume  = "Audio_SetChannelVolume";
constexpr const char* AudioStopChannel       = "Audio_StopChannel";
constexpr const char* WaitForAudioComplete   = "Audio_WaitComplete";

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
constexpr const char* MetaSetBool        = "Meta_SetBool";
constexpr const char* MetaGetBool        = "Meta_GetBool";
constexpr const char* MetaSetInt         = "Meta_SetInt";
constexpr const char* MetaGetInt         = "Meta_GetInt";
constexpr const char* MetaSetString      = "Meta_SetString";
constexpr const char* MetaGetString      = "Meta_GetString";

// Weather
constexpr const char* WeatherSet           = "Weather_Set";
constexpr const char* WeatherSetFog        = "Weather_SetFog";
constexpr const char* WeatherGetType       = "Weather_GetType";
constexpr const char* WeatherGetRain       = "Weather_GetRain";
constexpr const char* WeatherGetSnow       = "Weather_GetSnow";
constexpr const char* WeatherGetFogDensity = "Weather_GetFogDensity";
constexpr const char* WeatherIsLightning   = "Weather_IsLightning";
constexpr const char* WeatherSetWind       = "Weather_SetWind";
constexpr const char* WeatherGetWindStrength  = "Weather_GetWindStrength";
constexpr const char* WeatherGetWindDirection = "Weather_GetWindDirection";

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
constexpr const char* ParticlePreset       = "Particle_Preset";

// Destructible
constexpr const char* DestructibleDamage   = "Destructible_Damage";

// Prefab
constexpr const char* PrefabInstantiate    = "Prefab_Instantiate";

// UI
constexpr const char* UISetText            = "UI_SetText";
constexpr const char* UISetProgress        = "UI_SetProgress";
constexpr const char* UISetVisible         = "UI_SetElementVisible";
constexpr const char* UIIsChecked          = "UI_IsChecked";
constexpr const char* UIGetSliderValue     = "UI_GetSliderValue";
constexpr const char* UIIsHovered          = "UI_IsHovered";
constexpr const char* UIIsPressed          = "UI_IsPressed";
constexpr const char* UIGetText            = "UI_GetText";
constexpr const char* UIGetProgress        = "UI_GetProgress";
constexpr const char* UISetTextColor       = "UI_SetTextColor";
constexpr const char* UISetBgColor         = "UI_SetBgColor";
constexpr const char* UISetImageAlpha      = "UI_SetImageAlpha";

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
constexpr const char* NetGetPlayerCount    = "Net_GetPlayerCount";
constexpr const char* NetIsHost            = "Net_IsHost";
constexpr const char* NetGetLocalPlayerId  = "Net_GetLocalPlayerId";
constexpr const char* NetGetPacketLoss     = "Net_GetPacketLoss";
constexpr const char* NetSetReady          = "Net_SetReady";
constexpr const char* NetLobbyCount        = "Net_LobbyPlayerCount";
constexpr const char* NetLobbyName         = "Net_LobbyPlayerName";
constexpr const char* NetLobbyReady        = "Net_LobbyPlayerReady";

// UI Focus
constexpr const char* UISetFocus           = "UI_SetFocus";
constexpr const char* UIClearFocus         = "UI_ClearFocus";
constexpr const char* UIGetFocusedElement  = "UI_GetFocusedElement";

// Tween
constexpr const char* TweenPosition        = "Tween_Position";
constexpr const char* TweenRotation        = "Tween_Rotation";
constexpr const char* TweenScale           = "Tween_Scale";
constexpr const char* TweenFloat           = "Tween_Float";
constexpr const char* TweenOpacity         = "Tween_Opacity";
constexpr const char* TweenGetValue        = "Tween_GetValue";
constexpr const char* TweenStopAll         = "Tween_StopAll";

// Dialogue
constexpr const char* DialogueStart        = "Dialogue_Start";
constexpr const char* DialogueAdvance      = "Dialogue_Advance";
constexpr const char* DialogueIsActive     = "Dialogue_IsActive";
constexpr const char* DialogueChoose         = "Dialogue_Choose";
constexpr const char* DialogueGetChoiceCount = "Dialogue_GetChoiceCount";
constexpr const char* DialogueGetChoiceText  = "Dialogue_GetChoiceText";
constexpr const char* DialogueGetSpeaker     = "Dialogue_GetSpeaker";
constexpr const char* DialogueGetText        = "Dialogue_GetText";
constexpr const char* DialogueGetVariable    = "Dialogue_GetVariable";
constexpr const char* DialogueSetVariable    = "Dialogue_SetVariable";

// Animator (extended)
constexpr const char* AnimatorStop         = "Animator_Stop";
constexpr const char* AnimatorIsPlaying    = "Animator_IsPlaying";

// AI
constexpr const char* AISetTarget          = "AI_SetTarget";
constexpr const char* AIGetTarget          = "AI_GetTarget";
constexpr const char* AISetState           = "AI_SetState";
constexpr const char* AIGetState           = "AI_GetState";
constexpr const char* AISetMoveSpeed       = "AI_SetMoveSpeed";
constexpr const char* AIGetMoveSpeed       = "AI_GetMoveSpeed";
constexpr const char* AISetDetectionRange  = "AI_SetDetectionRange";
constexpr const char* AIGetDetectionRange  = "AI_GetDetectionRange";
constexpr const char* AISetAttackRange     = "AI_SetAttackRange";
constexpr const char* AIGetAttackRange     = "AI_GetAttackRange";
constexpr const char* AISetChaseSpeed      = "AI_SetChaseSpeed";
constexpr const char* AISetFleeSpeed       = "AI_SetFleeSpeed";
constexpr const char* AISetFieldOfView     = "AI_SetFieldOfView";
constexpr const char* AISetUseNavmesh      = "AI_SetUseNavmesh";
constexpr const char* AISetTargetPosition  = "AI_SetTargetPosition";

// Entity direction vectors
constexpr const char* EntityGetForward     = "Entity_GetForward";
constexpr const char* EntityGetRight       = "Entity_GetRight";
constexpr const char* EntityGetUp          = "Entity_GetUp";

// Behavior Tree
constexpr const char* BTEnable             = "BT_Enable";
constexpr const char* BTDisable            = "BT_Disable";
constexpr const char* BTReset              = "BT_Reset";
constexpr const char* BTGetEnabled         = "BT_GetEnabled";
constexpr const char* BTSetBBBool          = "BT_SetBlackboardBool";
constexpr const char* BTGetBBBool          = "BT_GetBlackboardBool";
constexpr const char* BTSetBBFloat         = "BT_SetBlackboardFloat";
constexpr const char* BTGetBBFloat         = "BT_GetBlackboardFloat";
constexpr const char* BTSetBBInt           = "BT_SetBlackboardInt";
constexpr const char* BTGetBBInt           = "BT_GetBlackboardInt";
constexpr const char* BTSetBBString        = "BT_SetBlackboardString";
constexpr const char* BTGetBBString        = "BT_GetBlackboardString";

// Physics joints
constexpr const char* JointCreateHinge     = "Joint_CreateHinge";
constexpr const char* JointCreateDistance  = "Joint_CreateDistance";
constexpr const char* JointDestroy         = "Joint_Destroy";
constexpr const char* JointHingeSetLimits  = "Joint_HingeSetLimits";
constexpr const char* JointHingeSetMotor   = "Joint_HingeSetMotor";
constexpr const char* JointHingeGetAngle   = "Joint_HingeGetAngle";
constexpr const char* JointDistanceSetRest = "Joint_DistanceSetRest";
constexpr const char* JointDistanceGetStress = "Joint_DistanceGetStress";

// Navmesh
constexpr const char* NavmeshIsPointOn     = "Navmesh_IsPointOnNavmesh";
constexpr const char* NavmeshHasNavmesh    = "Navmesh_HasNavmesh";
constexpr const char* NavmeshFindPath      = "Navmesh_FindPath";
constexpr const char* NavmeshGetWaypoint   = "Navmesh_GetWaypoint";
constexpr const char* NavmeshPathExists    = "Navmesh_PathExists";

// State Machine
constexpr const char* SMSetState           = "StateMachine_SetState";
constexpr const char* SMGetState           = "StateMachine_GetState";

// Accessibility
constexpr const char* SubtitleShow         = "Subtitle_Show";
constexpr const char* AnnouncerAnnounce    = "Announcer_Announce";
constexpr const char* ColorblindSetMode    = "Colorblind_SetMode";
constexpr const char* A11ySetFontScale        = "A11y_SetFontScale";
constexpr const char* A11yGetFontScale        = "A11y_GetFontScale";
constexpr const char* A11ySetReducedMotion    = "A11y_SetReducedMotion";
constexpr const char* A11yGetReducedMotion    = "A11y_GetReducedMotion";
constexpr const char* A11ySetScreenShake      = "A11y_SetScreenShake";
constexpr const char* A11yGetScreenShake      = "A11y_GetScreenShake";
constexpr const char* A11ySetContrast         = "A11y_SetContrast";
constexpr const char* A11yGetContrast         = "A11y_GetContrast";
constexpr const char* A11ySetColorblindStrength = "A11y_SetColorblindStrength";
constexpr const char* A11yGetColorblindStrength = "A11y_GetColorblindStrength";
constexpr const char* A11ySetSubtitles        = "A11y_SetSubtitles";
constexpr const char* A11yGetSubtitles        = "A11y_GetSubtitles";
constexpr const char* A11ySetDyslexiaFont     = "A11y_SetDyslexiaFont";
constexpr const char* A11yGetDyslexiaFont     = "A11y_GetDyslexiaFont";
constexpr const char* A11ySetScreenReader     = "A11y_SetScreenReader";
constexpr const char* A11yGetScreenReader     = "A11y_GetScreenReader";
constexpr const char* A11ySave                = "A11y_Save";

// Input
constexpr const char* InputIsKeyPressed    = "Input_IsKeyPressed";
constexpr const char* InputIsKeyDown       = "Input_IsKeyDown";
constexpr const char* InputGetMousePosition = "Input_GetMousePosition";
constexpr const char* InputGetAxis         = "Input_GetAxis";

// Scene Management
constexpr const char* SceneLoadScene       = "Scene_LoadScene";
constexpr const char* SceneGetCurrentScene = "Scene_GetCurrentScene";

// Noise
constexpr const char* NoisePerlin2D        = "Noise_Perlin2D";
constexpr const char* NoiseSimplex2D       = "Noise_Simplex2D";
constexpr const char* NoiseWorley2D        = "Noise_Worley2D";
constexpr const char* NoiseFBM2D           = "Noise_FBM2D";
constexpr const char* NoisePerlin3D        = "Noise_Perlin3D";
constexpr const char* NoiseSimplex3D       = "Noise_Simplex3D";

// Procedural
constexpr const char* ProceduralCellularAutomata = "Procedural_CellularAutomata";
constexpr const char* ProceduralRandomWalker     = "Procedural_RandomWalker";
constexpr const char* ProceduralBSP              = "Procedural_BSP";
constexpr const char* ProceduralDiamondSquare    = "Procedural_DiamondSquare";
constexpr const char* ProceduralLSystem          = "Procedural_LSystem";
constexpr const char* ProceduralVoronoi          = "Procedural_Voronoi";
constexpr const char* ProceduralWFC              = "Procedural_WFC";
constexpr const char* ProceduralGrammar          = "Procedural_Grammar";
constexpr const char* ProceduralSpawnGrid        = "Procedural_SpawnGrid";

// Streaming
constexpr const char* StreamingForceLoad   = "Streaming_ForceLoad";
constexpr const char* StreamingForceUnload = "Streaming_ForceUnload";
constexpr const char* StreamingGetState    = "Streaming_GetState";
constexpr const char* StreamingIsLoaded    = "Streaming_IsLoaded";
constexpr const char* StreamingGetLoadedCount = "Streaming_GetLoadedCount";
constexpr const char* StreamingSetEnabled  = "Streaming_SetEnabled";

// Audio Event Graph
constexpr const char* AudioGraphTriggerEvent = "AudioGraph_TriggerEvent";
constexpr const char* AudioGraphSetParameter = "AudioGraph_SetParameter";
constexpr const char* AudioGraphStopAll      = "AudioGraph_StopAll";

// Plugin
constexpr const char* PluginIsLoaded  = "Plugin_IsLoaded";
constexpr const char* PluginLoad      = "Plugin_Load";
constexpr const char* PluginUnload    = "Plugin_Unload";

// Water
constexpr const char* WaterSetStyle          = "Water_SetStyle";
constexpr const char* WaterSetWaveHeight     = "Water_SetWaveHeight";
constexpr const char* WaterSetWaveSpeed      = "Water_SetWaveSpeed";
constexpr const char* WaterGetWaveHeight     = "Water_GetWaveHeight";
constexpr const char* WaterSetOpacity        = "Water_SetOpacity";
constexpr const char* WaterSetColor          = "Water_SetColor";

// HUD
constexpr const char* HUDSetEnabled  = "HUD_SetEnabled";
constexpr const char* HUDIsEnabled   = "HUD_IsEnabled";
constexpr const char* HUDSetVisible  = "HUD_SetVisible";
constexpr const char* HUDSetText     = "HUD_SetText";
constexpr const char* HUDSetValue    = "HUD_SetValue";
constexpr const char* HUDSetFillColor = "HUD_SetFillColor";
constexpr const char* HUDSetTextColor = "HUD_SetTextColor";
constexpr const char* HUDSetFontSize  = "HUD_SetFontSize";
constexpr const char* HUDSetPosition  = "HUD_SetPosition";

// Text
constexpr const char* TextSetContent = "Text_SetContent";
constexpr const char* TextSetColor   = "Text_SetColor";

// Sprite2D
constexpr const char* SpriteSetTexture       = "Sprite_SetTexture";
constexpr const char* SpriteSetColor         = "Sprite_SetColor";
constexpr const char* SpriteSetFlip          = "Sprite_SetFlip";
constexpr const char* SpriteGetFlip          = "Sprite_GetFlip";
constexpr const char* SpriteAnimPlay         = "SpriteAnim_Play";
constexpr const char* SpriteAnimStop         = "SpriteAnim_Stop";
constexpr const char* SpriteAnimSetSpeed     = "SpriteAnim_SetSpeed";
constexpr const char* SpriteAnimIsPlaying    = "SpriteAnim_IsPlaying";
constexpr const char* SpriteAnimGetFrame     = "SpriteAnim_GetFrame";

// Light
constexpr const char* LightSetColor          = "Light_SetColor";
constexpr const char* LightSetIntensity      = "Light_SetIntensity";
constexpr const char* LightGetIntensity      = "Light_GetIntensity";

// Camera
constexpr const char* CameraSetFOV           = "Camera_SetFOV";
constexpr const char* CameraSetActive        = "Camera_SetActive";

// Material
constexpr const char* MaterialSetColor       = "Material_SetColor";
constexpr const char* MaterialSetEmissive    = "Material_SetEmissive";
constexpr const char* MaterialSetTransmission = "Material_SetTransmission";
constexpr const char* MaterialSetSSS         = "Material_SetSSS";

// Visibility
constexpr const char* EntitySetVisible       = "Entity_SetVisible";
constexpr const char* EntityIsVisible        = "Entity_IsVisible";

// Object Pool
constexpr const char* PoolAcquire            = "Pool_Acquire";
constexpr const char* PoolRelease            = "Pool_Release";

// Save Data
constexpr const char* SaveDataSet            = "SaveData_Set";
constexpr const char* SaveDataGet            = "SaveData_Get";
constexpr const char* SaveDataHasTag         = "SaveData_HasTag";

// Rigidbody
constexpr const char* PhysicsSetKinematic    = "Physics_SetKinematic";
constexpr const char* PhysicsGetAngularVelocity = "Physics_GetAngularVelocity";

// Post-Process Volume
constexpr const char* PPVolumeSetActive      = "PPVolume_SetActive";
constexpr const char* PPVolumeSetWeight      = "PPVolume_SetWeight";
constexpr const char* PPVolumeSetBlendRadius = "PPVolume_SetBlendRadius";
constexpr const char* PPVolumeSetPriority    = "PPVolume_SetPriority";

// Screen-Space Effects
constexpr const char* SSEffectSetSSAO             = "SSEffect_SetSSAO";
constexpr const char* SSEffectSetContactShadows   = "SSEffect_SetContactShadows";
constexpr const char* SSEffectSetGodRays          = "SSEffect_SetGodRays";
constexpr const char* SSEffectSetCaustics         = "SSEffect_SetCaustics";
constexpr const char* SSEffectSetFogShafts        = "SSEffect_SetFogShafts";

// Elemental System
constexpr const char* ElementalSpawnFire          = "Elemental_SpawnFire";
constexpr const char* ElementalSpawnWater         = "Elemental_SpawnWater";
constexpr const char* ElementalSpawnSnow          = "Elemental_SpawnSnow";
constexpr const char* ElementalSpawnSteam         = "Elemental_SpawnSteam";
constexpr const char* ElementalSpawnDebris        = "Elemental_SpawnDebris";
constexpr const char* ElementalGetFireIntensity   = "Elemental_GetFireIntensity";
constexpr const char* ElementalGetMoisture        = "Elemental_GetMoisture";
constexpr const char* ElementalGetActiveCount     = "Elemental_GetActiveCount";
constexpr const char* ElementalSetEmitterActive   = "Elemental_SetEmitterActive";
constexpr const char* ElementalSetEmitterElement  = "Elemental_SetEmitterElement";
constexpr const char* ElementalSetFlammability    = "Elemental_SetFlammability";
constexpr const char* ElementalGetSurfaceState    = "Elemental_GetSurfaceState";

} // namespace NodeTypes

} // namespace VisualScript
} // namespace Enjin
