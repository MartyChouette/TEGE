#include "Enjin/Networking/NewgroundsSaveBackend.h"
#include "Enjin/Logging/Log.h"
#include <functional>

namespace Enjin {
namespace Networking {

NewgroundsSaveBackend::NewgroundsSaveBackend(NewgroundsAPI* api)
    : m_Api(api) {}

i32 NewgroundsSaveBackend::GetSlotId(const std::string& key) const {
    // Hash key to NG slot range [1..10]
    std::hash<std::string> h;
    return static_cast<i32>((h(key) % 10) + 1);
}

bool NewgroundsSaveBackend::Write(const std::string& key, const std::string& data) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = GetSlotId(key);
    return m_Api->SaveSlot(slot, data);
}

bool NewgroundsSaveBackend::Read(const std::string& key, std::string& outData) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = GetSlotId(key);
    outData = m_Api->LoadSlot(slot);
    return !outData.empty();
}

bool NewgroundsSaveBackend::Delete(const std::string& key) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = GetSlotId(key);
    return m_Api->SaveSlot(slot, "");
}

bool NewgroundsSaveBackend::Exists(const std::string& key) {
    if (!m_Api || !m_Api->IsConnected()) return false;
    i32 slot = GetSlotId(key);
    std::string data = m_Api->LoadSlot(slot);
    return !data.empty();
}

} // namespace Networking
} // namespace Enjin
