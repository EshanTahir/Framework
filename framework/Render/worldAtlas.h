#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Packs small world prop PNGs into GPU atlas page(s) so DrawSprites can
// share one bind. Sky / player sheets stay out (callers simply don't pack them).
struct WorldAtlasEntry
{
    unsigned int gl_id = 0;
    float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
    int page = 0;
    int px_w = 0, px_h = 0;
};

class WorldAtlas
{
public:
    static WorldAtlas& GetInstance();

    // Pack these logical paths (e.g. "res/world/tiled/ladder.png"). Idempotent.
    void EnsurePaths(const std::vector<std::string>& paths);

    // Product registers the content set once at boot; EnsureContentPaths packs it.
    void SetContentPaths(std::vector<std::string> paths);
    void EnsureContentPaths();

    bool TryGet(const char* path, WorldAtlasEntry& out) const;
    bool TryGet(const std::string& path, WorldAtlasEntry& out) const;

    int PageCount() const { return (int)m_pages.size(); }
    void Clear();

private:
    WorldAtlas() = default;

    struct Page
    {
        unsigned int gl_id = 0;
        int size = 0;
        int shelf_y = 0;
        int shelf_h = 0;
        int cursor_x = 0;
        std::vector<unsigned char> pixels; // RGBA size*size
    };

    std::vector<Page> m_pages;
    std::unordered_map<std::string, WorldAtlasEntry> m_entries;
    std::vector<std::string> m_content_paths;

    static constexpr int kPageSize = 1024;
    // 1px gutter around each stamp. StampRgba extrudes edge texels into it.
    static constexpr int kPad = 1;

    Page& AllocPage();
    bool StampRgba(Page& page, const unsigned char* src, int sw, int sh, int src_pitch, int& out_x, int& out_y);
    void UploadPage(Page& page);
};
