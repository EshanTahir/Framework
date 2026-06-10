#pragma once

#include <string>

// Resolves asset paths: FW_ASSET_ROOT, else walk up from exe/cwd until a folder
// containing res/ is found. User data (settings/saves) uses SDL pref path.
namespace Paths
{
    void Init();
    // Optional: set before Init to control SDL_GetPrefPath(org, app).
    void SetUserDataIdentity(const std::string& org, const std::string& app);

    void SetAssetRoot(const std::string& root);
    const std::string& GetAssetRoot();

    // Writable prefs/saves root (created on EnsureUserDataDir).
    const std::string& GetUserDataRoot();
    bool EnsureUserDataDir();

    // Accepts "res/foo.png", "../../res/foo.png", or absolute paths.
    std::string Resolve(const std::string& path);
    std::string Resolve(const char* path);
}
