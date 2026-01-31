#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace Enjin::Scene
{
    struct SaveSlotInfo
    {
        u32 slotIndex = 0;
        std::string displayName;
        std::string timestamp;
        std::string scenePath;
        f32 playTime = 0.0f;
        bool occupied = false;
    };

    struct GameSaveData
    {
        std::string scenePath;
        Math::Vector3 playerPosition;
        f32 playerHealth = 100.0f;

        std::vector<std::pair<std::string, f32>> floatVars;
        std::vector<std::pair<std::string, i32>> intVars;
        std::vector<std::pair<std::string, bool>> boolVars;
        std::vector<std::pair<std::string, std::string>> stringVars;

        f32 playTime = 0.0f;
        std::string saveTimestamp;

        void SetFloat(const std::string& name, f32 value)
        {
            for (auto& pair : floatVars)
            {
                if (pair.first == name) { pair.second = value; return; }
            }
            floatVars.emplace_back(name, value);
        }

        void SetInt(const std::string& name, i32 value)
        {
            for (auto& pair : intVars)
            {
                if (pair.first == name) { pair.second = value; return; }
            }
            intVars.emplace_back(name, value);
        }

        void SetBool(const std::string& name, bool value)
        {
            for (auto& pair : boolVars)
            {
                if (pair.first == name) { pair.second = value; return; }
            }
            boolVars.emplace_back(name, value);
        }

        void SetString(const std::string& name, const std::string& value)
        {
            for (auto& pair : stringVars)
            {
                if (pair.first == name) { pair.second = value; return; }
            }
            stringVars.emplace_back(name, value);
        }

        f32 GetFloat(const std::string& name, f32 defaultValue = 0.0f) const
        {
            for (const auto& pair : floatVars)
            {
                if (pair.first == name) return pair.second;
            }
            return defaultValue;
        }

        i32 GetInt(const std::string& name, i32 defaultValue = 0) const
        {
            for (const auto& pair : intVars)
            {
                if (pair.first == name) return pair.second;
            }
            return defaultValue;
        }

        bool GetBool(const std::string& name, bool defaultValue = false) const
        {
            for (const auto& pair : boolVars)
            {
                if (pair.first == name) return pair.second;
            }
            return defaultValue;
        }

        std::string GetString(const std::string& name, const std::string& defaultValue = "") const
        {
            for (const auto& pair : stringVars)
            {
                if (pair.first == name) return pair.second;
            }
            return defaultValue;
        }
    };

    class ENJIN_API GameSaveSystem
    {
    public:
        static constexpr u32 MAX_SLOTS = 10;

        bool SaveToSlot(u32 slot, const GameSaveData& data);
        bool LoadFromSlot(u32 slot, GameSaveData& outData);
        bool DeleteSlot(u32 slot);

        SaveSlotInfo GetSlotInfo(u32 slot) const;
        std::vector<SaveSlotInfo> GetAllSlots() const;

        static std::string GetSaveDirectory();

    private:
        std::string GetSlotPath(u32 slot) const;
        nlohmann::json DataToJson(const GameSaveData& data) const;
        GameSaveData JsonToData(const nlohmann::json& j) const;
    };

} // namespace Enjin::Scene
