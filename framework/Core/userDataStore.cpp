#include "userDataStore.h"
#include "log.h"
#include "paths.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace UserDataStore
{
    std::string SlotPath(const char* prefix, int slot)
    {
        if (!prefix || !prefix[0])
            prefix = "slot";
        if (slot < 0)
            slot = 0;
        return Paths::GetUserDataRoot() + "/" + prefix + "_" + std::to_string(slot) + ".json";
    }

    bool Exists(const std::string& path)
    {
        std::ifstream in(path);
        return (bool)in;
    }

    bool Delete(const std::string& path)
    {
        std::error_code ec;
        return fs::remove(path, ec) || !ec;
    }

    bool ReadJson(const std::string& path, nlohmann::json& out)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        try
        {
            in >> out;
            return true;
        }
        catch (const std::exception& e)
        {
            FW_CORE_ERROR("UserDataStore read failed ({0}): {1}", path, e.what());
            return false;
        }
    }

    bool WriteJson(const std::string& path, const nlohmann::json& j, int indent)
    {
        Paths::EnsureUserDataDir();
        std::ofstream out(path);
        if (!out)
        {
            FW_CORE_ERROR("UserDataStore: cannot write {0}", path);
            return false;
        }
        out << j.dump(indent);
        return true;
    }
}
