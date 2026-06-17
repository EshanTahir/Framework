#include "Render/worldAtlas.h"
#include "Core/assetCache.h"
#include "Core/log.h"
#include "Core/paths.h"
#include "Render/gl.h"

#include "SDL.h"
#include "SDL_image.h"

#include <algorithm>
#include <cstring>

WorldAtlas& WorldAtlas::GetInstance()
{
    static WorldAtlas instance;
    return instance;
}

void WorldAtlas::Clear()
{
    for (auto& p : m_pages)
    {
        if (p.gl_id && glDeleteTextures)
            glDeleteTextures(1, &p.gl_id);
    }
    m_pages.clear();
    m_entries.clear();
}

WorldAtlas::Page& WorldAtlas::AllocPage()
{
    Page page;
    page.size = kPageSize;
    page.pixels.assign((size_t)kPageSize * (size_t)kPageSize * 4u, 0);
    page.shelf_y = kPad;
    page.shelf_h = 0;
    page.cursor_x = kPad;
    if (glGenTextures)
    {
        glGenTextures(1, &page.gl_id);
        glBindTexture(GL_TEXTURE_2D, page.gl_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kPageSize, kPageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    m_pages.push_back(std::move(page));
    return m_pages.back();
}

void WorldAtlas::UploadPage(Page& page)
{
    if (!page.gl_id || !glBindTexture || !glTexImage2D)
        return;
    glBindTexture(GL_TEXTURE_2D, page.gl_id);
    // Loader GL subset has no glTexSubImage2D - full reupload is fine for rare packs.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, page.size, page.size, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool WorldAtlas::StampRgba(Page& page, const unsigned char* src, int sw, int sh, int src_pitch, int& out_x, int& out_y)
{
    if (sw <= 0 || sh <= 0 || !src)
        return false;
    const int need_w = sw + kPad;
    const int need_h = sh + kPad;

    if (page.cursor_x + need_w > page.size - kPad)
    {
        page.shelf_y += page.shelf_h + kPad;
        page.cursor_x = kPad;
        page.shelf_h = 0;
    }
    if (page.shelf_y + need_h > page.size - kPad)
        return false;

    out_x = page.cursor_x;
    out_y = page.shelf_y;

    for (int y = 0; y < sh; ++y)
    {
        const unsigned char* row = src + y * src_pitch;
        unsigned char* dst = page.pixels.data() + ((size_t)(out_y + y) * (size_t)page.size + (size_t)out_x) * 4u;
        std::memcpy(dst, row, (size_t)sw * 4u);
    }

    // Copy edge texels into the 1px gutter so NEAREST at UV 0/1 samples the rim,
    // not empty pad (one UV path - no 0.5-texel inset / "eat").
    auto px = [&](int x, int y) -> unsigned char* {
        return page.pixels.data() + ((size_t)y * (size_t)page.size + (size_t)x) * 4u;
    };
    for (int y = 0; y < sh; ++y)
    {
        std::memcpy(px(out_x - 1, out_y + y), px(out_x, out_y + y), 4);
        std::memcpy(px(out_x + sw, out_y + y), px(out_x + sw - 1, out_y + y), 4);
    }
    for (int x = -1; x <= sw; ++x)
    {
        std::memcpy(px(out_x + x, out_y - 1), px(out_x + x, out_y), 4);
        std::memcpy(px(out_x + x, out_y + sh), px(out_x + x, out_y + sh - 1), 4);
    }

    page.cursor_x += need_w;
    page.shelf_h = std::max(page.shelf_h, need_h);
    return true;
}

void WorldAtlas::EnsurePaths(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        if (path.empty())
            continue;
        const std::string resolved = Paths::Resolve(path);
        if (m_entries.find(resolved) != m_entries.end())
            continue;

        CachedTexture& src_tex = AssetCache::GetInstance().GetTexture(path.c_str());
        if (!src_tex.surface)
        {
            FW_CORE_ERROR("WorldAtlas: missing surface for {0}", path);
            continue;
        }

        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(src_tex.surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (!rgba)
        {
            FW_CORE_ERROR("WorldAtlas: convert failed for {0}", path);
            continue;
        }

        if (m_pages.empty())
            AllocPage();

        int ox = 0, oy = 0;
        bool stamped = StampRgba(m_pages.back(), (const unsigned char*)rgba->pixels, rgba->w, rgba->h, rgba->pitch, ox, oy);
        if (!stamped)
        {
            AllocPage();
            stamped = StampRgba(m_pages.back(), (const unsigned char*)rgba->pixels, rgba->w, rgba->h, rgba->pitch, ox, oy);
        }
        if (!stamped)
        {
            FW_CORE_ERROR("WorldAtlas: image too large for page ({0}x{1}) {2}", rgba->w, rgba->h, path);
            SDL_FreeSurface(rgba);
            continue;
        }

        Page& page = m_pages.back();
        UploadPage(page);

        WorldAtlasEntry entry;
        entry.gl_id = page.gl_id;
        entry.page = (int)m_pages.size() - 1;
        entry.px_w = rgba->w;
        entry.px_h = rgba->h;
        const float inv = 1.f / (float)page.size;
        entry.u0 = (float)ox * inv;
        entry.v0 = (float)oy * inv;
        entry.u1 = (float)(ox + rgba->w) * inv;
        entry.v1 = (float)(oy + rgba->h) * inv;

        m_entries[resolved] = entry;
        // Also index by the logical path the scripts use.
        m_entries[path] = entry;
        m_entries[Paths::Resolve(path.c_str())] = entry;

        SDL_FreeSurface(rgba);
        FW_CORE_INFO("WorldAtlas: packed {0} -> page {1} ({2},{3}) {4}x{5}",
                     path, entry.page, ox, oy, entry.px_w, entry.px_h);
    }
}

void WorldAtlas::SetContentPaths(std::vector<std::string> paths)
{
    m_content_paths = std::move(paths);
}

void WorldAtlas::EnsureContentPaths()
{
    if (m_content_paths.empty())
        return;
    EnsurePaths(m_content_paths);
}

bool WorldAtlas::TryGet(const char* path, WorldAtlasEntry& out) const
{
    if (!path)
        return false;
    thread_local std::string key;
    key.assign(path);
    return TryGet(key, out);
}

bool WorldAtlas::TryGet(const std::string& path, WorldAtlasEntry& out) const
{
    auto it = m_entries.find(path);
    if (it == m_entries.end())
    {
        const std::string resolved = Paths::Resolve(path);
        it = m_entries.find(resolved);
        if (it == m_entries.end())
            return false;
    }
    out = it->second;
    return out.gl_id != 0;
}
