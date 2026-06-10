#include "paths.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    std::string g_asset_root;
    std::string g_user_data;
    std::string g_pref_org = "Game";
    std::string g_pref_app = "Game";

    std::string NormalizeSlashes(std::string s)
    {
        std::replace(s.begin(), s.end(), '\\', '/');
        return s;
    }

    std::string StripRelativePrefix(std::string p)
    {
        p = NormalizeSlashes(p);
        while (p.rfind("../", 0) == 0)
            p = p.substr(3);
        while (p.rfind("./", 0) == 0)
            p = p.substr(2);
        return p;
    }

    // Walk start and parents until a directory containing res/ is found.
    bool TrySetRootWithRes(fs::path start)
    {
        std::error_code ec;
        fs::path cur = fs::weakly_canonical(start, ec);
        if (ec)
            cur = start;
        for (int i = 0; i < 8; ++i)
        {
            if (fs::exists(cur / "res"))
            {
                Paths::SetAssetRoot(cur.string());
                return true;
            }
            if (!cur.has_parent_path() || cur == cur.parent_path())
                break;
            cur = cur.parent_path();
        }
        return false;
    }
}

namespace Paths
{
    void SetAssetRoot(const std::string& root)
    {
        g_asset_root = NormalizeSlashes(root);
        while (!g_asset_root.empty() && (g_asset_root.back() == '/' || g_asset_root.back() == '\\'))
            g_asset_root.pop_back();
    }

    const std::string& GetAssetRoot()
    {
        return g_asset_root;
    }

    const std::string& GetUserDataRoot()
    {
        return g_user_data;
    }

    bool EnsureUserDataDir()
    {
        if (g_user_data.empty())
            return false;
        std::error_code ec;
        fs::create_directories(g_user_data, ec);
        return !ec;
    }

    void SetUserDataIdentity(const std::string& org, const std::string& app)
    {
        if (!org.empty())
            g_pref_org = org;
        if (!app.empty())
            g_pref_app = app;
    }

    void Init()
    {
        if (const char* env = std::getenv("FW_ASSET_ROOT"))
        {
            SetAssetRoot(env);
        }
        else
        {
            char* base = SDL_GetBasePath();
            if (base)
            {
                fs::path exe_dir(base);
                SDL_free(base);
                TrySetRootWithRes(exe_dir);
            }

            if (g_asset_root.empty())
                TrySetRootWithRes(fs::current_path());

            if (g_asset_root.empty())
                SetAssetRoot(fs::current_path().string());
        }

        if (char* pref = SDL_GetPrefPath(g_pref_org.c_str(), g_pref_app.c_str()))
        {
            g_user_data = NormalizeSlashes(pref);
            while (!g_user_data.empty() && (g_user_data.back() == '/' || g_user_data.back() == '\\'))
                g_user_data.pop_back();
            SDL_free(pref);
        }
        else
        {
            g_user_data = g_asset_root + "/userdata";
        }
        EnsureUserDataDir();
    }

    std::string Resolve(const char* path)
    {
        return Resolve(std::string(path ? path : ""));
    }

    std::string Resolve(const std::string& path)
    {
        if (path.empty())
            return path;

        fs::path p(path);
        if (p.is_absolute() && fs::exists(p))
            return NormalizeSlashes(p.string());

        std::string stripped = StripRelativePrefix(path);
        fs::path joined = fs::path(g_asset_root) / stripped;
        if (fs::exists(joined))
            return NormalizeSlashes(fs::weakly_canonical(joined).string());

        if (fs::exists(path))
            return NormalizeSlashes(fs::weakly_canonical(path).string());

        return NormalizeSlashes(joined.string());
    }
}
