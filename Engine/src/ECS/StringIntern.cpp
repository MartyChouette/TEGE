#include "Enjin/ECS/StringIntern.h"

namespace Enjin {
namespace ECS {

u32 StringIntern::Intern(const std::string& str) {
    auto it = m_Map.find(str);
    if (it != m_Map.end()) {
        return it->second;
    }
    u32 id = static_cast<u32>(m_Strings.size());
    m_Strings.push_back(str);
    m_Map[str] = id;
    return id;
}

const std::string& StringIntern::GetString(u32 id) const {
    if (id < static_cast<u32>(m_Strings.size())) {
        return m_Strings[id];
    }
    // Return empty string (ID 0) for out-of-range
    return m_Strings[0];
}

bool StringIntern::HasString(const std::string& str) const {
    return m_Map.find(str) != m_Map.end();
}

u32 StringIntern::Find(const std::string& str) const {
    auto it = m_Map.find(str);
    if (it != m_Map.end()) {
        return it->second;
    }
    return 0;
}

void StringIntern::Clear() {
    m_Map.clear();
    m_Strings.clear();
    m_Strings.push_back(""); // Restore ID 0 = empty string
}

} // namespace ECS
} // namespace Enjin
