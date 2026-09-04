#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef ENJIN_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#endif

namespace Enjin {
namespace Accessibility {

AccessibilityAnnouncer::~AccessibilityAnnouncer() {
#ifdef ENJIN_PLATFORM_WINDOWS
    if (m_SAPIVoice) {
        static_cast<ISpVoice*>(m_SAPIVoice)->Release();
        m_SAPIVoice = nullptr;
    }
    if (m_SAPICOMOwned) {
        CoUninitialize();
        m_SAPICOMOwned = false;
    }
#endif
}

#ifdef ENJIN_PLATFORM_WINDOWS
// Lazy-create the SAPI voice on first speak. All failures are silent — no TTS
// device just means the visual status bar carries the announcement alone.
// RPC_E_CHANGED_MODE means another apartment already initialized COM on this
// thread; the voice can still be created, we just don't own the uninit.
static ISpVoice* AcquireSAPIVoice(void*& slot, bool& comOwned) {
    if (slot) return static_cast<ISpVoice*>(slot);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        comOwned = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return nullptr;
    }
    ISpVoice* voice = nullptr;
    hr = CoCreateInstance(__uuidof(SpVoice), nullptr, CLSCTX_ALL,
                          __uuidof(ISpVoice), reinterpret_cast<void**>(&voice));
    if (FAILED(hr)) {
        if (comOwned) { CoUninitialize(); comOwned = false; }
        return nullptr;
    }
    slot = voice;
    return voice;
}
#endif

void AccessibilityAnnouncer::Announce(const std::string& text, AnnouncePriority priority) {
    Announcement ann;
    ann.text = text;
    ann.priority = priority;
    ann.timeRemaining = displayDuration;

    // High/Critical announcements go to front of queue
    if (priority >= AnnouncePriority::High) {
        m_Queue.push_front(ann);
    } else {
        m_Queue.push_back(ann);
    }

    // Limit queue size
    while (m_Queue.size() > 5) {
        m_Queue.pop_back();
    }

    if (logToConsole) {
        const char* priorityStr = "";
        switch (priority) {
            case AnnouncePriority::Low: priorityStr = "INFO"; break;
            case AnnouncePriority::Normal: priorityStr = "ACTION"; break;
            case AnnouncePriority::High: priorityStr = "WARNING"; break;
            case AnnouncePriority::Critical: priorityStr = "CRITICAL"; break;
        }
        ENJIN_LOG_INFO(Editor, "[Announce/%s] %s", priorityStr, text.c_str());
    }

#ifdef __EMSCRIPTEN__
    // In the browser the announcer can actually speak: Web Speech API.
    // High/Critical announcements interrupt whatever is being spoken.
    if (enabled) {
        EM_ASM({
            if (!window.speechSynthesis) return;
            // Safari (and Chrome's autoplay policy) keep speech muted until a
            // user gesture has unlocked it, and iOS never unlocks it on its
            // own. Without this the screen reader is silent for the whole
            // session on an iPhone with no error anywhere. Speaking an empty
            // utterance from the first touch is the unlock; it is armed once
            // and removes itself.
            if (!window.__enjinSpeechUnlocked) {
                window.__enjinSpeechUnlocked = true;
                var unlock = function () {
                    try { window.speechSynthesis.speak(new SpeechSynthesisUtterance('')); } catch (e) {}
                    window.removeEventListener('touchend', unlock, true);
                    window.removeEventListener('pointerup', unlock, true);
                    window.removeEventListener('keydown', unlock, true);
                };
                window.addEventListener('touchend', unlock, true);
                window.addEventListener('pointerup', unlock, true);
                window.addEventListener('keydown', unlock, true);
            }
            if ($1) { window.speechSynthesis.cancel(); }
            var u = new SpeechSynthesisUtterance(UTF8ToString($0));
            u.rate = 1.0;
            u.volume = 1.0;
            window.speechSynthesis.speak(u);
        }, text.c_str(), priority >= AnnouncePriority::High ? 1 : 0);
    }
#endif

#ifdef ENJIN_PLATFORM_WINDOWS
    // Desktop Windows speaks through SAPI — same semantics as the web branch:
    // async, High/Critical purge whatever is mid-sentence.
    if (enabled) {
        if (ISpVoice* voice = AcquireSAPIVoice(m_SAPIVoice, m_SAPICOMOwned)) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wtext(static_cast<size_t>(wlen), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);
                DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
                if (priority >= AnnouncePriority::High) flags |= SPF_PURGEBEFORESPEAK;
                voice->Speak(wtext.c_str(), flags, nullptr);
            }
        }
    }
#endif
}

void AccessibilityAnnouncer::Update(f32 dt) {
    for (auto& ann : m_Queue) {
        ann.timeRemaining -= dt;
    }

    m_Queue.erase(
        std::remove_if(m_Queue.begin(), m_Queue.end(),
            [](const Announcement& a) { return a.timeRemaining <= 0.0f; }),
        m_Queue.end());
}

void AccessibilityAnnouncer::RenderStatusBar() {
    if (!enabled || m_Queue.empty()) return;

    const auto& current = m_Queue.front();

    // Draw at bottom of screen. The bar scales with the accessibility font
    // scale: a player who needs larger text needs it here most of all, since
    // this bar is the screen reader's visible output.
    ImGuiIO& io = ImGui::GetIO();
    const f32 scale = (fontScale > 0.1f) ? fontScale : 1.0f;
    const f32 fontSize = ImGui::GetFontSize() * scale;
    f32 barHeight = fontSize + ImGui::GetStyle().ItemSpacing.y + 8.0f;
    ImVec2 barPos(0.0f, io.DisplaySize.y - barHeight);
    ImVec2 barEnd(io.DisplaySize.x, io.DisplaySize.y);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Background
    // T-M6: Guard against negative timeRemaining producing negative alpha
    f32 alpha = std::min(1.0f, std::max(0.0f, current.timeRemaining) / 0.5f);
    ImU32 bgCol = IM_COL32(20, 20, 30, static_cast<u8>(200 * alpha));
    drawList->AddRectFilled(barPos, barEnd, bgCol);

    // Priority color indicator
    ImU32 indicatorCol;
    switch (current.priority) {
        case AnnouncePriority::Low:      indicatorCol = IM_COL32(100, 180, 255, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::Normal:   indicatorCol = IM_COL32(100, 220, 100, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::High:     indicatorCol = IM_COL32(255, 180, 50, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::Critical: indicatorCol = IM_COL32(255, 80, 80, static_cast<u8>(255 * alpha)); break;
    }
    drawList->AddRectFilled(barPos, ImVec2(barPos.x + 4.0f, barEnd.y), indicatorCol);

    // Text
    ImU32 textCol = IM_COL32(220, 220, 220, static_cast<u8>(255 * alpha));
    drawList->AddText(ImGui::GetFont(), fontSize,
                      ImVec2(barPos.x + 12.0f, barPos.y + 4.0f), textCol, current.text.c_str());
}

const std::string& AccessibilityAnnouncer::GetCurrentText() const {
    if (m_Queue.empty()) return m_EmptyString;
    return m_Queue.front().text;
}

void AccessibilityAnnouncer::Clear() {
    m_Queue.clear();
}

} // namespace Accessibility
} // namespace Enjin
