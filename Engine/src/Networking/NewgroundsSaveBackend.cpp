#include "Enjin/Networking/NewgroundsSaveBackend.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>

namespace Enjin {
namespace Networking {

NewgroundsSaveBackend::NewgroundsSaveBackend(NewgroundsAPI* api)
    : m_Api(api) {}

void NewgroundsSaveBackend::LoadMetadata() {
    m_KeyToSlot.clear();
    m_MetadataLoaded = true;

    if (!m_Api || !m_Api->IsConnected()) return;

    std::string raw = m_Api->LoadSlot(METADATA_SLOT);
    if (raw.empty()) return;

    try {
        auto j = nlohmann::json::parse(raw);
        if (j.is_object() && j.contains("keys")) {
            for (auto& [key, val] : j["keys"].items()) {
                if (val.is_number_integer()) {
                    i32 slot = val.get<i32>();
                    if (slot >= FIRST_DATA_SLOT && slot <= LAST_DATA_SLOT) {
                        m_KeyToSlot[key] = slot;
                    }
                }
            }
        }
    } catch (...) {
        ENJIN_LOG_WARN(Network, "NewgroundsSaveBackend: Failed to parse metadata slot");
    }
}

void NewgroundsSaveBackend::SaveMetadata() {
    if (!m_Api || !m_Api->IsConnected()) return;

    nlohmann::json j;
    j["keys"] = nlohmann::json::object();
    for (const auto& [key, slot] : m_KeyToSlot) {
        j["keys"][key] = slot;
    }
    m_Api->SaveSlot(METADATA_SLOT, j.dump());
}

i32 NewgroundsSaveBackend::ResolveSlot(const std::string& key, bool allocateIfMissing) {
    if (!m_MetadataLoaded) LoadMetadata();

    auto it = m_KeyToSlot.find(key);
    if (it != m_KeyToSlot.end()) return it->second;

    if (!allocateIfMissing) return 0;

    // Find next free slot (2-10)
    for (i32 slot = FIRST_DATA_SLOT; slot <= LAST_DATA_SLOT; slot++) {
        bool used = false;
        for (const auto& [k, s] : m_KeyToSlot) {
            if (s == slot) { used = true; break; }
        }
        if (!used) {
            m_KeyToSlot[key] = slot;
            SaveMetadata();
            return slot;
        }
    }

    ENJIN_LOG_WARN(Network, "NewgroundsSaveBackend: All 9 data slots in use, cannot store key '%s'", key.c_str());
    return 0;
}

bool NewgroundsSaveBackend::Write(const std::string& key, const std::string& data) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = ResolveSlot(key, true);
    if (slot == 0) return false;
    return m_Api->SaveSlot(slot, data);
}

bool NewgroundsSaveBackend::Read(const std::string& key, std::string& outData) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = ResolveSlot(key, false);
    if (slot == 0) return false;
    outData = m_Api->LoadSlot(slot);
    return !outData.empty();
}

bool NewgroundsSaveBackend::Delete(const std::string& key) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = ResolveSlot(key, false);
    if (slot == 0) return true; // Key never existed
    m_Api->SaveSlot(slot, "");
    m_KeyToSlot.erase(key);
    SaveMetadata();
    return true;
}

bool NewgroundsSaveBackend::Exists(const std::string& key) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = ResolveSlot(key, false);
    if (slot == 0) return false;
    std::string data = m_Api->LoadSlot(slot);
    return !data.empty();
}

} // namespace Networking
} // namespace Enjin
