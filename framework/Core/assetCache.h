#pragma once

#include "fmod.h"
#include "SDL.h"
#include "SDL_ttf.h"

#include <json.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

struct CachedTexture
{
    SDL_Surface* surface = nullptr; // kept for GL upload / sheet cutting
    unsigned int gl_id = 0;
    int width = 0;
    int height = 0;
};

class AssetCache
{
public:
    static AssetCache& GetInstance();

    bool enable_hot_reload = false;

    CachedTexture& GetTexture(const char* path);
    SDL_Surface* GetSurface(const char* path);

    TTF_Font* GetFont(const char* path, int size);
    FMOD_SOUND* GetSound(FMOD_SYSTEM* system, const char* path);
    const nlohmann::json& GetJson(const char* path);

    void Invalidate(const char* path);
    // Drop cached GPU textures so next GetTexture reloads from disk (fonts/sounds kept).
    void ReloadTextures();
    void Clear();

    unsigned int GetGLTexture(const char* path);

private:
    AssetCache() = default;
    ~AssetCache();

    std::unordered_map<std::string, CachedTexture> m_textures;
    std::unordered_map<std::string, TTF_Font*> m_fonts;
    std::unordered_map<std::string, FMOD_SOUND*> m_sounds;
    std::unordered_map<std::string, nlohmann::json> m_json;
    std::unordered_map<std::string, std::uintmax_t> m_mtimes;

    bool ShouldReload(const std::string& resolved);
    void TouchMtime(const std::string& resolved);
};
