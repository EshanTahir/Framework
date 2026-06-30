#pragma once

#include "box2d.h"
#include "fmod.h"
#include "SDL.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Asset caching: see Core/assetCache.h (hot-reload via AssetCache::enable_hot_reload)

enum class ScriptLanguage
{
    None = 0,
    Native,
    Luau
};

enum Shape
{
    box = 0,
    circle = 1,
    polygon = 2,
    edge = 3,
    chain = 4
};

struct TagComponent
{
    std::string tag;
};

struct ScriptComponent
{
    ScriptLanguage language = ScriptLanguage::Native;
    std::string path;
    uint32_t self = static_cast<uint32_t>(-1); // entt::entity underlying

    bool entering_scene = true;
    bool exiting_scene = false;
    bool loaded = false;
    bool booted = false; // true after first on_enter; hot-reload calls on_reload instead

    // Native fallback (bootstrap / demo)
    std::function<void()> OnEnter = []() {};
    std::function<void(float)> Update = [](float) {};
    std::function<void()> OnExit = []() {};

    ~ScriptComponent() { exiting_scene = true; }
};

struct TransformComponent
{
    float x = 0, y = 0, a = 0, s = 1;
    // Render interpolation: sim writes x/y/a; draw uses lerp(ix/iy/ia → x/y/a, alpha).
    float ix = 0, iy = 0, ia = 0;
    bool interp_primed = false;
};

// Off-world hide (depth park, hide-pool, soft-destroy). Matches Luau `x < -500`.
inline bool IsParkedWorldPos(float x, float y)
{
    return x < -500.f || y < -500.f;
}

struct RectComponent
{
    float x = 0, y = 0;
    int w = 0, h = 0;

    float top = 0, left = 0, bottom = 0, right = 0;
    float topleft[2]{}, bottomleft[2]{}, topright[2]{}, bottomright[2]{};
    float midtop[2]{}, midleft[2]{}, midbottom[2]{}, midright[2]{};
    float center[2]{};
    float centerx = 0, centery = 0;

    bool draw = false;
    bool moveable = false;
};

struct ColorComponent
{
    SDL_Color primary = {255, 0, 0, 255};
    SDL_Color secondary = {0, 255, 0, 255};
    SDL_Color tertiary = {0, 0, 255, 255};
};

struct FontComponent
{
    const char* font_location = nullptr;
    int size = 8;
    bool aa = true;
};

struct TextComponent
{
    std::string text = "sample text";
    bool camera_space = false; // legacy; prefer screen_space for UI
    bool screen_space = false; // true = draw in pixel coords (menu / HUD overlay)
    int sort_key = 0;
    // Cached glyph atlas - rebuilt when text/font/size/aa change
    unsigned int cached_tex = 0;
    int cached_w = 0;
    int cached_h = 0;
    std::string cache_key;
};

// Slot ownership for a dynamic body driven by InputSampler / scripts.
struct PlayerInputComponent
{
    int slot = 0;
    int device = 0; // 0 = keyboard, 1 = pad
    int pad_index = -1;
    int pad_instance = -1; // stable across hot-plugging; pad_index is not
};

struct CollideMapComponent
{
    const char* collide_map_location = nullptr;
    bool initialized = false;
    std::vector<b2Body*> bodies; // owned tile collision bodies (destroyed on uninit)
};

struct MaterialSheetComponent
{
    const char* location = nullptr;
};

struct TileMapComponent
{
    const char* tile_map_location = nullptr;
    bool collision_built = false;
};

struct SpriteSheetComponent
{
    std::string path_owned; // when set, image_location points here
    const char* image_location = nullptr;
    std::string palette_owned; // optional UV palette PNG; when set, palette_location points here
    const char* palette_location = nullptr;
    float ppu = 1.f;
    bool flip_vertical = false;
    bool flip_horizontal = false;
    float angle_offset = 0.f; // visual only (e.g. grapple aim); does not rotate body
    float offset_x = 0.f;
    float offset_y = 0.f;
    bool align_feet = true; // pin sprite bottom to box bottom (Y+ down)
    bool visible = true;
    std::vector<SDL_Rect> sprites;
    int rows = 1;
    int columns = 1;
    // Dirty cache for CutSprites - rebuild only when layout/path changes
    const char* cut_image = nullptr;
    int cut_rows = -1;
    int cut_columns = -1;
};

struct SpriteComponent
{
    std::string path_owned; // when set, image_location points here
    const char* image_location = nullptr;
    float ppu = 1.f;
    bool flip_vertical = false;
    bool flip_horizontal = false;
    int sort_key = 0;
};

// One visual for a WxH tile flood (ladder / pole / blob sheets). Drawn as N quads, one entity.
struct SpriteGridComponent
{
    std::string body_path_owned;
    std::string top_path_owned;
    const char* body_path = nullptr;
    const char* top_path = nullptr; // null → all body
    int cols = 1;
    int rows = 1;
    float cell = 1.f; // world units per cell
    float ppu = 2.f;
    int sort_key = -1000;
    bool visible = true;
    // sheet_cols > 0: body_path is a cell sheet; frames[iy*cols+ix] = sheet index (-1 skip).
    int sheet_cols = 0;
    int sheet_rows = 0;
    std::vector<int> frames;
};

struct AnimationComponent
{
    std::vector<SpriteComponent> frames;
    float current_frame = 0;
    float frame_rate = 24;
    bool play = false;
    bool looping = true;
    bool enabled = true;
    int frame_start = 0;  // inclusive index into sheet.sprites
    int frame_end = -1;   // inclusive; -1 = last cut frame
    std::string state = "idle";
};

struct SoundComponent
{
    const char* sound_location = nullptr;
    FMOD_CHANNEL* channel = nullptr;
    bool play = false;
    bool is_looping = false;
    bool oneshot = true;
    float volume = 1;
    float pitch = 1;
    float pan = 0; // -1..1 stereo (fake 2D spatial); 0 = center
    int bus = 1; // 0=master 1=sfx 2=music 3=ui (matches AudioBus routing in soundSystem)
};

struct ViewPortComponent
{
    int depth = 1;
    int width = 0;
    int height = 0;
};

struct CameraComponent
{
    int depth = 1;
    float zoom = 1;
    float shake_time = 0;
    float shake_mag = 0;
};

// Presentation: draw layer + camera parallax. Missing = band 0, parallax 0.
struct SortBandComponent
{
    int band = 0;         // lower draws first (background)
    float parallax = 0.f; // 0 = locked to gameplay cam, 1 = infinite-distance sky
};

// Soft mood wash (AABB). Cosmetic only - no gameplay collision.
struct LightVolumeComponent
{
    float half_w = 1.f;
    float half_h = 1.f;
    float r = 1.f, g = 0.95f, b = 0.85f;
    float intensity = 0.35f; // alpha of wash
    int priority = 0;
    bool multiply = false; // if true, use multiply blend; else additive/alpha wash
};

// Marks a sprite/solid as contributing to bloom threshold (drawn brighter).
struct EmissiveComponent
{
    float strength = 1.2f; // multiplies RGB when drawing
};

// Additive beam (world-space). Transform = origin; angle on transform.
struct LightShaftComponent
{
    float length = 4.f;
    float width = 1.2f;
    float r = 1.f, g = 0.95f, b = 0.7f, a = 0.2f;
    float scroll = 0.f; // optional phase
};

// Nested "postcard" view. view_cam = entity with Camera+Transform looking elsewhere.
struct ApertureComponent
{
    bool active = true;
    float half_w = 1.5f;
    float half_h = 1.0f;
    uint32_t view_cam = 0; // entt entity id; 0 = unset
    int sort_key = 100;    // draw frame after nested contents
};

struct BoxShapeComponent
{
    float width = 1;
    float height = 1;
    b2PolygonShape* shape = nullptr;
};

struct CircleShapeComponent
{
    float radius = 1;
    b2CircleShape* shape = nullptr;
};

struct PolygonShapeComponent
{
    std::vector<b2Vec2> vertices;
    b2PolygonShape* shape = nullptr;
};

struct EdgeShapeComponent
{
    b2Vec2 vertex1{};
    b2Vec2 vertex2{};
    b2EdgeShape* shape = nullptr;
};

struct ChainShapeComponent
{
    std::vector<b2Vec2> vertices;
    bool loop = false;
    b2ChainShape* shape = nullptr;
};

struct MaterialComponent
{
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
};

struct RigidBody2DComponent
{
    b2BodyType type = b2_dynamicBody;
    b2Vec2 velocity = {0, 0};
    float angular_velocity = 0;
    float gravity_scale = 1.0f;
    bool fixed_rotation = false;
    bool bullet = false; // CCD - helps fast cars vs walls
    b2Body* body = nullptr;
    bool initialized = false;

    // Legacy: still accepted if no Collider2DComponent
    Shape shape = box;
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;
};

struct Collider2DComponent
{
    Shape shape = box;
    bool sensor = false;
    uint16_t category_bits = 0x0001;
    uint16_t mask_bits = 0xFFFF;
    b2Fixture* fixture = nullptr;
    bool is_ground_sensor = false;
    bool one_way = false; // pass through from below (movers / elevs / fall plats)
};

struct CollisionComponent
{
    bool on_ground = false;
    std::vector<uint32_t> touching; // entity ids
};

struct ParticleEmitterComponent
{
    const char* image_location = nullptr; // null => white debug texture
    float rate = 20;
    float life = 0.5f;
    float speed = 2;
    float spread = 3.14f;
    float angle = -1.5708f; // radians, emission cone center (default: up, Y+ down)
    float size = 0.25f;
    float accum = 0;
    bool emitting = true;
    int burst = 0; // one-shot extra particles this frame
    float r = 1, g = 1, b = 1, a = 1;
    float offset_x = 0; // spawn offset from transform (e.g. feet)
    float offset_y = 0;
    bool volume_spawn = false; // randomize spawn inside BoxShape
};

struct DebugDrawComponent
{
    bool filled = true;
    bool outline = true;
    int sort = 0; // lower draws first (bounce pads behind solids)
};

struct Particle
{
    float x, y, vx, vy, life, max_life, size;
};

struct ParticleBufferComponent
{
    std::vector<Particle> particles;
};

struct ButtonComponent
{
    bool active = true;
    int state = 0;
    std::string text = "Button";
    // Procedural button palette (idle / hover / pressed)
    ColorComponent color;              // idle fill
    ColorComponent hover_color;
    ColorComponent selected_color;
    ColorComponent text_color;
    ColorComponent hover_text_color;
    ColorComponent selected_text_color;
    ColorComponent outline_color;
    const char* font_location = nullptr;
    int font_size = 28;
    bool aa_font = true;
    bool outline = true;
    float outline_width = 4.f; // pixels
    // 0 = center (default), 1 = left-padded (console left-rail menus)
    int text_align = 0;
    // Overlay panels (pause dim): drawn under screen text; no hover accent.
    bool overlay = false;
    bool accent = true;
    Uint8 click_button = SDL_BUTTON_LEFT;
    RectComponent rect;
    RectComponent outline_rect;
    bool clicked_down = false;
    // Sticky until ui_clicked() consumes - survives 0-sim display frames.
    bool clicked_up = false;
    bool hover = false;
    bool pressed = false; // held while inside after mouse-down
    unsigned int cached_tex = 0;
    int cached_w = 0;
    int cached_h = 0;
    std::string cache_key;
};

struct DebugHudComponent
{
    bool show = true;
};
