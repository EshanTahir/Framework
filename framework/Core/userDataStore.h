#pragma once

#include "json.hpp"

#include <string>

// Generic JSON files under the user-data root (prefs/saves). No product schema here.
namespace UserDataStore
{
    // e.g. prefix "save" → <userData>/save_0.json
    std::string SlotPath(const char* prefix, int slot);

    bool Exists(const std::string& path);
    bool Delete(const std::string& path);

    bool ReadJson(const std::string& path, nlohmann::json& out);
    bool WriteJson(const std::string& path, const nlohmann::json& j, int indent = 2);
}
