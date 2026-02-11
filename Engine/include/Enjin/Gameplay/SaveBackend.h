#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Gameplay {

// Abstract interface for save data storage backends
class ENJIN_API ISaveBackend {
public:
    virtual ~ISaveBackend() = default;
    virtual bool Write(const std::string& key, const std::string& data) = 0;
    virtual bool Read(const std::string& key, std::string& outData) = 0;
    virtual bool Delete(const std::string& key) = 0;
    virtual bool Exists(const std::string& key) = 0;
    virtual std::string GetName() const = 0;
};

// Local file system save backend
class ENJIN_API LocalSaveBackend : public ISaveBackend {
public:
    explicit LocalSaveBackend(const std::string& baseDirectory = "");

    bool Write(const std::string& key, const std::string& data) override;
    bool Read(const std::string& key, std::string& outData) override;
    bool Delete(const std::string& key) override;
    bool Exists(const std::string& key) override;
    std::string GetName() const override { return "Local"; }

private:
    std::string GetFilePath(const std::string& key) const;
    std::string m_BaseDirectory;
};

} // namespace Gameplay
} // namespace Enjin
