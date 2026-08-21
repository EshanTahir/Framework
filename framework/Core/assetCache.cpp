#include "assetCache.h"
#include "paths.h"
#include "log.h"
#include "Render/gl.h"

#include "SDL_image.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

AssetCache& AssetCache::GetInstance()
{
    static AssetCache instance;
    return instance;
}

AssetCache::~AssetCache()
{
    Clear();
}

bool AssetCache::ShouldReload(const std::string& resolved)
{
    if (!enable_hot_reload)
        return false;
    std::error_code ec;
    auto mtime = fs::last_write_time(resolved, ec);
    if (ec)
        return false;
    auto count = static_cast<std::uintmax_t>(mtime.time_since_epoch().count());
    auto it = m_mtimes.find(resolved);
    if (it == m_mtimes.end())
        return false;
    return it->second != count;
}

void AssetCache::TouchMtime(const std::string& resolved)
{
    std::error_code ec;
    auto mtime = fs::last_write_time(resolved, ec);
    if (!ec)
        m_mtimes[resolved] = static_cast<std::uintmax_t>(mtime.time_since_epoch().count());
}

CachedTexture& AssetCache::GetTexture(const char* path)
{
    std::string resolved = Paths::Resolve(path);
    if (ShouldReload(resolved))
        Invalidate(path);

    auto it = m_textures.find(resolved);
    if (it != m_textures.end())
        return it->second;

    CachedTexture tex{};
    tex.surface = IMG_Load(resolved.c_str());
    if (!tex.surface)
    {
        FW_CORE_ERROR("Failed to load image '{0}': {1}", resolved, IMG_GetError());
        tex.gl_id = 0;
        tex.width = 0;
        tex.height = 0;
        m_textures[resolved] = tex;
        return m_textures[resolved];
    }

    tex.width = tex.surface->w;
    tex.height = tex.surface->h;

#ifdef FW_USE_OPENGL
    if (glGenTextures)
    {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(tex.surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (converted)
        {
            glGenTextures(1, &tex.gl_id);
            glBindTexture(GL_TEXTURE_2D, tex.gl_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
            tex.width = converted->w;
            tex.height = converted->h;
            SDL_FreeSurface(converted);
        }
    }
#endif

    TouchMtime(resolved);
    m_textures[resolved] = tex;
    return m_textures[resolved];
}

bool AssetCache::HasUsableTexture(const char* path)
{
    if (!path || !path[0])
        return false;
    CachedTexture& tex = GetTexture(path);
    return tex.gl_id != 0 && tex.width > 0 && tex.height > 0;
}

SDL_Surface* AssetCache::GetSurface(const char* path)
{
    return GetTexture(path).surface;
}

unsigned int AssetCache::GetGLTexture(const char* path)
{
    return GetTexture(path).gl_id;
}

TTF_Font* AssetCache::GetFont(const char* path, int size)
{
    std::string resolved = Paths::Resolve(path);
    std::string key = resolved + "#" + std::to_string(size);

    if (ShouldReload(resolved))
    {
        auto it = m_fonts.find(key);
        if (it != m_fonts.end())
        {
            TTF_CloseFont(it->second);
            m_fonts.erase(it);
        }
    }

    auto it = m_fonts.find(key);
    if (it != m_fonts.end())
        return it->second;

    TTF_Font* font = TTF_OpenFont(resolved.c_str(), size);
    if (!font)
        FW_CORE_ERROR("Failed to load font '{0}': {1}", resolved, TTF_GetError());

    TouchMtime(resolved);
    m_fonts[key] = font;
    return font;
}

FMOD_SOUND* AssetCache::GetSound(FMOD_SYSTEM* system, const char* path)
{
    std::string resolved = Paths::Resolve(path);

    if (ShouldReload(resolved))
    {
        auto it = m_sounds.find(resolved);
        if (it != m_sounds.end() && it->second)
        {
            FMOD_Sound_Release(it->second);
            m_sounds.erase(it);
        }
    }

    auto it = m_sounds.find(resolved);
    if (it != m_sounds.end())
        return it->second;

    FMOD_SOUND* sound = nullptr;
    FMOD_RESULT r = FMOD_System_CreateSound(system, resolved.c_str(), FMOD_DEFAULT, 0, &sound);
    if (r != FMOD_OK)
        FW_CORE_ERROR("Failed to load sound '{0}' (FMOD {1})", resolved, (int)r);

    TouchMtime(resolved);
    m_sounds[resolved] = sound;
    return sound;
}

const nlohmann::json& AssetCache::GetJson(const char* path)
{
    std::string resolved = Paths::Resolve(path);

    if (ShouldReload(resolved))
        m_json.erase(resolved);

    auto it = m_json.find(resolved);
    if (it != m_json.end())
        return it->second;

    nlohmann::json j;
    std::ifstream file(resolved);
    if (file.is_open())
    {
        try
        {
            file >> j;
        }
        catch (const std::exception& e)
        {
            FW_CORE_ERROR("JSON parse failed '{0}': {1}", resolved, e.what());
        }
    }
    else
    {
        FW_CORE_ERROR("Failed to open JSON '{0}'", resolved);
    }

    TouchMtime(resolved);
    m_json[resolved] = std::move(j);
    return m_json[resolved];
}

void AssetCache::Invalidate(const char* path)
{
    std::string resolved = Paths::Resolve(path);
    auto it = m_textures.find(resolved);
    if (it != m_textures.end())
    {
        if (it->second.surface)
            SDL_FreeSurface(it->second.surface);
#ifdef FW_USE_OPENGL
        if (it->second.gl_id)
            glDeleteTextures(1, &it->second.gl_id);
#endif
        m_textures.erase(it);
    }
    m_json.erase(resolved);
    m_sounds.erase(resolved);
    m_mtimes.erase(resolved);
}

void AssetCache::ReloadTextures()
{
    for (auto& [path, tex] : m_textures)
    {
        if (tex.surface)
            SDL_FreeSurface(tex.surface);
#ifdef FW_USE_OPENGL
        if (tex.gl_id)
            glDeleteTextures(1, &tex.gl_id);
#endif
        m_mtimes.erase(path);
    }
    m_textures.clear();
}

void AssetCache::Clear()
{
    for (auto& [_, tex] : m_textures)
    {
        if (tex.surface)
            SDL_FreeSurface(tex.surface);
#ifdef FW_USE_OPENGL
        if (tex.gl_id)
            glDeleteTextures(1, &tex.gl_id);
#endif
    }
    m_textures.clear();

    for (auto& [_, font] : m_fonts)
    {
        if (font)
            TTF_CloseFont(font);
    }
    m_fonts.clear();

    for (auto& [_, sound] : m_sounds)
    {
        if (sound)
            FMOD_Sound_Release(sound);
    }
    m_sounds.clear();
    m_json.clear();
    m_mtimes.clear();
}
