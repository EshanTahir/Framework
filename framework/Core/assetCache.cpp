#include "assetCache.h"
#include "paths.h"
#include "log.h"

#include "SDL_image.h"

#include <filesystem>

namespace fs = std::filesystem;

AssetCache& AssetCache::GetInstance()
{
    static AssetCache instance;
    return instance;
}

AssetCache::~AssetCache() { Clear(); }

CachedTexture& AssetCache::GetTexture(const char* path)
{
    static CachedTexture empty;
    if (!path || !path[0]) return empty;
    std::string key = path;
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    CachedTexture tex;
    const std::string resolved = Paths::Resolve(path);
    tex.surface = IMG_Load(resolved.c_str());
    m_textures[key] = tex;
    return m_textures[key];
}

SDL_Surface* AssetCache::GetSurface(const char* path) { return GetTexture(path).surface; }

TTF_Font* AssetCache::GetFont(const char* path, int size)
{
    if (!path) return nullptr;
    const std::string key = std::string(path) + "#" + std::to_string(size);
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) return it->second;
    const std::string resolved = Paths::Resolve(path);
    TTF_Font* font = TTF_OpenFont(resolved.c_str(), size);
    m_fonts[key] = font;
    return font;
}

FMOD_SOUND* AssetCache::GetSound(FMOD_SYSTEM* system, const char* path)
{
    (void)system; (void)path; return nullptr;
}

const nlohmann::json& AssetCache::GetJson(const char* path)
{
    static nlohmann::json empty = nlohmann::json::object();
    if (!path) return empty;
    auto it = m_json.find(path);
    if (it != m_json.end()) return it->second;
    m_json[path] = nlohmann::json::object();
    return m_json[path];
}

void AssetCache::Invalidate(const char* path)
{
    if (!path) return;
    m_textures.erase(path);
    m_json.erase(path);
}

void AssetCache::ReloadTextures() { m_textures.clear(); }

void AssetCache::Clear()
{
    for (auto& [_, tex] : m_textures)
    {
        if (tex.surface) SDL_FreeSurface(tex.surface);
    }
    m_textures.clear();
    for (auto& [_, font] : m_fonts)
    {
        if (font) TTF_CloseFont(font);
    }
    m_fonts.clear();
    m_sounds.clear();
    m_json.clear();
    m_mtimes.clear();
}

unsigned int AssetCache::GetGLTexture(const char* path) { return GetTexture(path).gl_id; }

bool AssetCache::ShouldReload(const std::string&) { return false; }
void AssetCache::TouchMtime(const std::string&) {}
