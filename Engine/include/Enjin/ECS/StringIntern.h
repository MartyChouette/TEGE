#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace ECS {

class StringIntern {
public:
    static StringIntern& Instance() {
        static StringIntern inst;
        return inst;
    }

    u32 Intern(const std::string& str);
    const std::string& GetString(u32 id) const;
    bool HasString(const std::string& str) const;
    u32 Find(const std::string& str) const; // Returns 0 if not found
    void Clear();
    usize Count() const { return m_Strings.size(); }

private:
    StringIntern() { m_Strings.push_back(""); } // ID 0 = empty string
    StringIntern(const StringIntern&) = delete;
    StringIntern& operator=(const StringIntern&) = delete;

    std::unordered_map<std::string, u32> m_Map;
    std::vector<std::string> m_Strings;
};

} // namespace ECS
} // namespace Enjin
