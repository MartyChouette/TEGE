#include "Enjin/Platform/SteamInput.h"
#include "Enjin/Logging/Log.h"

#include <cstring>

// When ENJIN_STEAM is defined, include the Steamworks SDK headers.
// The real implementation wraps ISteamInput calls.
// Without ENJIN_STEAM, all methods are safe no-ops that return default values.
#ifdef ENJIN_STEAM
// Steamworks SDK would be included here:
// #include <steam/steam_api.h>
// #include <steam/isteaminput.h>
#endif

namespace Enjin::Platform {

// =============================================================================
// Lifecycle
// =============================================================================

bool SteamInputManager::Initialize() {
    if (m_Initialized) return true;

#ifdef ENJIN_STEAM
    // Real implementation:
    // if (!SteamAPI_Init()) return false;
    // ISteamInput* input = SteamInput();
    // if (!input) return false;
    // input->Init(false);
    // m_Initialized = true;
    // ENJIN_LOG_INFO(Core, "Steam Input initialized");
    // return true;

    // Stub until Steamworks SDK is linked:
    ENJIN_LOG_INFO(Core, "Steam Input: ENJIN_STEAM defined but Steamworks SDK not linked (stub mode)");
    m_Initialized = true;
    return true;
#else
    ENJIN_LOG_INFO(Core, "Steam Input: not available (ENJIN_STEAM not defined)");
    return false;
#endif
}

void SteamInputManager::Shutdown() {
    if (!m_Initialized) return;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (input) input->Shutdown();
#endif

    m_Initialized = false;
    m_ControllerCount = 0;
    m_ActionSetHandles.clear();
    m_DigitalActionHandles.clear();
    m_AnalogActionHandles.clear();
    std::memset(m_ActiveActionSets, 0, sizeof(m_ActiveActionSets));
    std::memset(m_ControllerHandles, 0, sizeof(m_ControllerHandles));

    ENJIN_LOG_INFO(Core, "Steam Input shut down");
}

// =============================================================================
// Input polling
// =============================================================================

void SteamInputManager::PollInput() {
    if (!m_Initialized) return;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (!input) return;
    //
    // // Run Steam Input callbacks
    // input->RunFrame();
    //
    // // Enumerate connected controllers
    // InputHandle_t handles[STEAM_INPUT_MAX_COUNT];
    // m_ControllerCount = input->GetConnectedControllers(handles);
    // for (u32 i = 0; i < m_ControllerCount && i < 4; ++i) {
    //     m_ControllerHandles[i] = handles[i];
    // }
    //
    // // Gyro callback
    // if (m_GyroCallback && m_ControllerCount > 0) {
    //     InputMotionData_t motion = input->GetMotionData(m_ControllerHandles[0]);
    //     m_GyroCallback(motion.rotVelX, motion.rotVelY, motion.rotVelZ);
    // }
#endif
}

// =============================================================================
// Action sets
// =============================================================================

u64 SteamInputManager::RegisterActionSet(const std::string& name) {
    if (!m_Initialized) return 0;

    auto it = m_ActionSetHandles.find(name);
    if (it != m_ActionSetHandles.end()) {
        return it->second;
    }

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // InputActionSetHandle_t handle = input->GetActionSetHandle(name.c_str());
    // m_ActionSetHandles[name] = handle;
    // return handle;

    // Stub: generate sequential handles
    u64 handle = static_cast<u64>(m_ActionSetHandles.size() + 1);
    m_ActionSetHandles[name] = handle;
    return handle;
#else
    return 0;
#endif
}

void SteamInputManager::ActivateActionSet(u64 setHandle, u32 controllerIndex) {
    if (!m_Initialized || controllerIndex >= 4) return;

    m_ActiveActionSets[controllerIndex] = setHandle;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     input->ActivateActionSet(m_ControllerHandles[controllerIndex], setHandle);
    // }
#else
    (void)setHandle;
#endif
}

u64 SteamInputManager::GetActiveActionSet(u32 controllerIndex) const {
    if (controllerIndex >= 4) return 0;
    return m_ActiveActionSets[controllerIndex];
}

// =============================================================================
// Digital actions
// =============================================================================

u64 SteamInputManager::RegisterDigitalAction(const std::string& name) {
    if (!m_Initialized) return 0;

    auto it = m_DigitalActionHandles.find(name);
    if (it != m_DigitalActionHandles.end()) {
        return it->second;
    }

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // InputDigitalActionHandle_t handle = input->GetDigitalActionHandle(name.c_str());
    // m_DigitalActionHandles[name] = handle;
    // return handle;

    u64 handle = static_cast<u64>(m_DigitalActionHandles.size() + 100);
    m_DigitalActionHandles[name] = handle;
    return handle;
#else
    return 0;
#endif
}

DigitalActionState SteamInputManager::GetDigitalActionState(u64 actionHandle, u32 controllerIndex) const {
    DigitalActionState state;
    if (!m_Initialized || controllerIndex >= 4) return state;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     InputDigitalActionData_t data = input->GetDigitalActionData(
    //         m_ControllerHandles[controllerIndex], actionHandle);
    //     state.active = data.bActive;
    //     state.pressed = data.bState;
    // }
    (void)actionHandle;
#else
    (void)actionHandle;
#endif

    return state;
}

bool SteamInputManager::IsActionPressed(const std::string& name, u32 controllerIndex) const {
    auto it = m_DigitalActionHandles.find(name);
    if (it == m_DigitalActionHandles.end()) return false;
    return GetDigitalActionState(it->second, controllerIndex).pressed;
}

// =============================================================================
// Analog actions
// =============================================================================

u64 SteamInputManager::RegisterAnalogAction(const std::string& name) {
    if (!m_Initialized) return 0;

    auto it = m_AnalogActionHandles.find(name);
    if (it != m_AnalogActionHandles.end()) {
        return it->second;
    }

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // InputAnalogActionHandle_t handle = input->GetAnalogActionHandle(name.c_str());
    // m_AnalogActionHandles[name] = handle;
    // return handle;

    u64 handle = static_cast<u64>(m_AnalogActionHandles.size() + 200);
    m_AnalogActionHandles[name] = handle;
    return handle;
#else
    return 0;
#endif
}

AnalogActionState SteamInputManager::GetAnalogActionState(u64 actionHandle, u32 controllerIndex) const {
    AnalogActionState state;
    if (!m_Initialized || controllerIndex >= 4) return state;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     InputAnalogActionData_t data = input->GetAnalogActionData(
    //         m_ControllerHandles[controllerIndex], actionHandle);
    //     state.active = data.bActive;
    //     state.x = data.x;
    //     state.y = data.y;
    // }
    (void)actionHandle;
#else
    (void)actionHandle;
#endif

    return state;
}

f32 SteamInputManager::GetActionValue(const std::string& name, u32 controllerIndex) const {
    auto it = m_AnalogActionHandles.find(name);
    if (it == m_AnalogActionHandles.end()) return 0.0f;
    return GetAnalogActionState(it->second, controllerIndex).x;
}

// =============================================================================
// Steam overlay
// =============================================================================

void SteamInputManager::ShowBindingPanel(u32 controllerIndex) {
    if (!m_Initialized) return;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     input->ShowBindingPanel(m_ControllerHandles[controllerIndex]);
    // }
#endif
    (void)controllerIndex;
    ENJIN_LOG_INFO(Core, "Steam Input: ShowBindingPanel requested (controller %u)", controllerIndex);
}

void SteamInputManager::ShowOverlay(const std::string& page) {
#ifdef ENJIN_STEAM
    // ISteamFriends* friends = SteamFriends();
    // if (friends) {
    //     if (page.empty()) friends->ActivateGameOverlay("steamcommunity");
    //     else friends->ActivateGameOverlayToWebPage(page.c_str());
    // }
    (void)page;
#else
    (void)page;
#endif
}

// =============================================================================
// Haptics
// =============================================================================

void SteamInputManager::TriggerHapticPulse(u32 controllerIndex, u32 pad, u16 durationMicroseconds) {
    if (!m_Initialized) return;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     ESteamControllerPad ePad = (pad == 0) ? k_ESteamControllerPad_Left : k_ESteamControllerPad_Right;
    //     input->TriggerHapticPulse(m_ControllerHandles[controllerIndex], ePad, durationMicroseconds);
    // }
#endif
    (void)controllerIndex;
    (void)pad;
    (void)durationMicroseconds;
}

void SteamInputManager::TriggerRepeatedHapticPulse(u32 controllerIndex, u32 pad,
                                                     u16 durationMicroseconds, u16 offMicroseconds,
                                                     u16 repeat) {
    if (!m_Initialized) return;

#ifdef ENJIN_STEAM
    // ISteamInput* input = SteamInput();
    // if (controllerIndex < m_ControllerCount) {
    //     ESteamControllerPad ePad = (pad == 0) ? k_ESteamControllerPad_Left : k_ESteamControllerPad_Right;
    //     input->TriggerRepeatedHapticPulse(m_ControllerHandles[controllerIndex], ePad,
    //         durationMicroseconds, offMicroseconds, repeat, 0);
    // }
#endif
    (void)controllerIndex;
    (void)pad;
    (void)durationMicroseconds;
    (void)offMicroseconds;
    (void)repeat;
}

} // namespace Enjin::Platform
