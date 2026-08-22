#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Core/assetCache.h"
#include "Core/glContext.h"
#include "Core/log.h"
#include "Core/paths.h"
#include "Render/presentPipeline.h"
#include "Render/renderer2d.h"
#include "Render/worldAtlas.h"
#include "Scene/entity.h"
#include "Scene/occluderWorld.h"
#include "Scene/sceneHandler.h"
#include "lua.h"
#include "lualib.h"
#include <cstring>
#include <string>

using ScriptInternal::g_registry;
using ScriptInternal::g_ropes;
using ScriptInternal::g_sources;
using ScriptInternal::g_runtimes;
using ScriptInternal::g_script_events;
using ScriptInternal::BodyOf;
using ScriptInternal::LiveRope;
using ScriptInternal::DropRopeFor;
using ScriptInternal::SyncBodyWorldPos;
using ScriptInternal::EntityFromId;
using ScriptInternal::FindByTag;
using ScriptInternal::ResolveArg;
using ScriptInternal::ResolveEntityArg;
using ScriptInternal::BusNameToId;
using ScriptInternal::RefreshSource;
using ScriptInternal::RopeHandle;

namespace {

int l_set_color(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int r = (int)luaL_checkinteger(L, 2);
    int g = (int)luaL_checkinteger(L, 3);
    int b = (int)luaL_checkinteger(L, 4);
    int a = (int)luaL_optinteger(L, 5, 255);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<ColorComponent>(e))
        g_registry->emplace<ColorComponent>(e);
    auto& c = g_registry->get<ColorComponent>(e);
    c.primary = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    return 0;
}

int l_set_clear_color(lua_State* L)
{
    // values > 1 treated as 0-255
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    float a = (float)luaL_optnumber(L, 4, 255.0);
    if (r > 1.f || g > 1.f || b > 1.f || a > 1.f)
    {
        r /= 255.f; g /= 255.f; b /= 255.f; a /= 255.f;
    }
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
        return 0;
    scene->renderer.clear_r = r;
    scene->renderer.clear_g = g;
    scene->renderer.clear_b = b;
    scene->renderer.clear_a = a;
    return 0;
}

int l_set_world_textures(lua_State* L)
{
    bool on = lua_toboolean(L, 1) != 0;
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
        return 0;
    scene->renderer.draw_world_textures = on;
    return 0;
}

int l_get_world_textures(lua_State* L)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    bool on = scene ? scene->renderer.draw_world_textures : true;
    lua_pushboolean(L, on ? 1 : 0);
    return 1;
}

int l_set_sort_band(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    int band = (int)luaL_checkinteger(L, 2);
    float para = (float)luaL_optnumber(L, 3, 0.0);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<SortBandComponent>(e))
        g_registry->emplace<SortBandComponent>(e);
    auto& sb = g_registry->get<SortBandComponent>(e);
    sb.band = band;
    sb.parallax = para;
    return 0;
}

int l_set_emissive(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    float strength = (float)luaL_optnumber(L, 2, 1.2);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<EmissiveComponent>(e))
        g_registry->emplace<EmissiveComponent>(e);
    g_registry->get<EmissiveComponent>(e).strength = strength;
    return 0;
}

static void NormRgb(float& r, float& g, float& b)
{
    if (r > 1.f || g > 1.f || b > 1.f) { r /= 255.f; g /= 255.f; b /= 255.f; }
}

int l_set_light_volume(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    float r = (float)luaL_checknumber(L, 2);
    float g = (float)luaL_checknumber(L, 3);
    float b = (float)luaL_checknumber(L, 4);
    float intensity = (float)luaL_checknumber(L, 5);
    bool multiply = lua_gettop(L) >= 6 && lua_toboolean(L, 6) != 0;
    NormRgb(r, g, b);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<LightVolumeComponent>(e))
        g_registry->emplace<LightVolumeComponent>(e);
    auto& lv = g_registry->get<LightVolumeComponent>(e);
    lv.r = r; lv.g = g; lv.b = b;
    lv.intensity = intensity;
    lv.multiply = multiply;
    return 0;
}

int l_set_light_volume_size(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    float hw = (float)luaL_checknumber(L, 2);
    float hh = (float)luaL_checknumber(L, 3);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<LightVolumeComponent>(e))
        g_registry->emplace<LightVolumeComponent>(e);
    auto& lv = g_registry->get<LightVolumeComponent>(e);
    lv.half_w = hw;
    lv.half_h = hh;
    return 0;
}

int l_set_light_shaft(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    float len = (float)luaL_checknumber(L, 2);
    float width = (float)luaL_checknumber(L, 3);
    float r = (float)luaL_checknumber(L, 4);
    float g = (float)luaL_checknumber(L, 5);
    float b = (float)luaL_checknumber(L, 6);
    float a = (float)luaL_checknumber(L, 7);
    NormRgb(r, g, b);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<LightShaftComponent>(e))
        g_registry->emplace<LightShaftComponent>(e);
    auto& sh = g_registry->get<LightShaftComponent>(e);
    sh.length = len;
    sh.width = width;
    sh.r = r; sh.g = g; sh.b = b; sh.a = a;
    return 0;
}

int l_set_light_fill(lua_State* L)
{
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    float strength = (float)luaL_checknumber(L, 4);
    NormRgb(r, g, b);
    PresentPipeline::GetInstance().SetLightFill(r, g, b, strength);
    return 0;
}

int l_get_light_fill(lua_State* L)
{
    float r, g, b, s;
    PresentPipeline::GetInstance().GetLightFill(r, g, b, s);
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    lua_pushnumber(L, s);
    return 4;
}

int l_rebuild_occluders(lua_State* L)
{
    (void)L;
    if (!g_registry) return 0;
    OccluderWorld::Get().RebuildStatics(*g_registry);
    return 0;
}

int l_set_debug_occluders(lua_State* L)
{
    OccluderWorld::Get().debug_draw = lua_toboolean(L, 1) != 0;
    return 0;
}

int l_spawn_point_light(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float radius = (float)luaL_checknumber(L, 4);
    float r = (float)luaL_checknumber(L, 5);
    float g = (float)luaL_checknumber(L, 6);
    float b = (float)luaL_checknumber(L, 7);
    float intensity = (float)luaL_checknumber(L, 8);
    bool casts = true;
    if (lua_gettop(L) >= 9)
        casts = lua_toboolean(L, 9) != 0;
    NormRgb(r, g, b);

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }

    Entity existing = scene->GetEntity(tag);
    Entity e = existing.GetHandle() != entt::null ? existing : scene->CreateEntity(tag);
    if (!e.HasComponent<TransformComponent>())
        e.AddComponent<TransformComponent>();
    if (!e.HasComponent<PointLightComponent>())
        e.AddComponent<PointLightComponent>();
    auto& t = e.GetComponent<TransformComponent>();
    t.x = x; t.y = y;
    auto& pl = e.GetComponent<PointLightComponent>();
    pl.radius = radius;
    pl.r = r; pl.g = g; pl.b = b;
    pl.intensity = intensity;
    pl.casts_shadow = casts;
    pl.enabled = true;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_set_point_light(lua_State* L)
{
    int id = (int)luaL_checkinteger(L, 1);
    float r = (float)luaL_checknumber(L, 2);
    float g = (float)luaL_checknumber(L, 3);
    float b = (float)luaL_checknumber(L, 4);
    float intensity = (float)luaL_checknumber(L, 5);
    float radius = (float)luaL_checknumber(L, 6);
    NormRgb(r, g, b);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e)) return 0;
    if (!g_registry->has<PointLightComponent>(e))
        g_registry->emplace<PointLightComponent>(e);
    auto& pl = g_registry->get<PointLightComponent>(e);
    pl.r = r; pl.g = g; pl.b = b;
    pl.intensity = intensity;
    pl.radius = radius;
    if (lua_gettop(L) >= 7)
        pl.casts_shadow = lua_toboolean(L, 7) != 0;
    return 0;
}

int l_spawn_light_volume(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float hw = (float)luaL_checknumber(L, 4);
    float hh = (float)luaL_checknumber(L, 5);
    float r = (float)luaL_optnumber(L, 6, 1.0);
    float g = (float)luaL_optnumber(L, 7, 0.95);
    float b = (float)luaL_optnumber(L, 8, 0.85);
    float intensity = (float)luaL_optnumber(L, 9, 0.35);
    bool multiply = lua_toboolean(L, 10) != 0;
    if (r > 1.f || g > 1.f || b > 1.f) { r /= 255.f; g /= 255.f; b /= 255.f; }

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }
    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<LightVolumeComponent>();
    e.GetComponent<TransformComponent>().x = x;
    e.GetComponent<TransformComponent>().y = y;
    auto& lv = e.GetComponent<LightVolumeComponent>();
    lv.half_w = hw;
    lv.half_h = hh;
    lv.r = r; lv.g = g; lv.b = b;
    lv.intensity = intensity;
    lv.multiply = multiply;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_spawn_light_shaft(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float angle = (float)luaL_checknumber(L, 4);
    float len = (float)luaL_optnumber(L, 5, 4.0);
    float width = (float)luaL_optnumber(L, 6, 1.2);
    float r = (float)luaL_optnumber(L, 7, 1.0);
    float g = (float)luaL_optnumber(L, 8, 0.95);
    float b = (float)luaL_optnumber(L, 9, 0.7);
    float a = (float)luaL_optnumber(L, 10, 0.22);
    if (r > 1.f || g > 1.f || b > 1.f) { r /= 255.f; g /= 255.f; b /= 255.f; }

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }
    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<LightShaftComponent>();
    auto& t = e.GetComponent<TransformComponent>();
    t.x = x; t.y = y; t.a = angle;
    auto& sh = e.GetComponent<LightShaftComponent>();
    sh.length = len; sh.width = width;
    sh.r = r; sh.g = g; sh.b = b; sh.a = a;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_spawn_aperture(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float hw = (float)luaL_checknumber(L, 4);
    float hh = (float)luaL_checknumber(L, 5);
    float vx = (float)luaL_checknumber(L, 6);
    float vy = (float)luaL_checknumber(L, 7);
    float zoom = (float)luaL_optnumber(L, 8, 1.0);

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }

    std::string cam_tag = std::string(tag) + "_cam";
    Entity cam = scene->CreateEntity(cam_tag);
    cam.AddComponent<TransformComponent>();
    cam.AddComponent<CameraComponent>();
    cam.GetComponent<TransformComponent>().x = vx;
    cam.GetComponent<TransformComponent>().y = vy;
    cam.GetComponent<CameraComponent>().zoom = zoom;
    cam.GetComponent<CameraComponent>().depth = -100; // never master

    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<ApertureComponent>();
    e.AddComponent<ColorComponent>();
    e.AddComponent<DebugDrawComponent>();
    e.AddComponent<BoxShapeComponent>();
    e.GetComponent<TransformComponent>().x = x;
    e.GetComponent<TransformComponent>().y = y;
    e.GetComponent<ColorComponent>().primary = {40, 40, 50, 180};
    e.GetComponent<DebugDrawComponent>().filled = false;
    e.GetComponent<DebugDrawComponent>().outline = true;
    e.GetComponent<DebugDrawComponent>().sort = 200;
    e.GetComponent<BoxShapeComponent>().width = hw * 2.f;
    e.GetComponent<BoxShapeComponent>().height = hh * 2.f;
    auto& ap = e.GetComponent<ApertureComponent>();
    ap.half_w = hw;
    ap.half_h = hh;
    ap.view_cam = (uint32_t)cam.GetHandle();
    ap.active = true;

    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_shake_camera(lua_State* L)
{
    float mag = (float)luaL_optnumber(L, 1, 0.3);
    float time = (float)luaL_optnumber(L, 2, 0.15);
    auto e = FindByTag("camera");
    if (e == entt::null || !g_registry || !g_registry->has<CameraComponent>(e))
        return 0;
    auto& c = g_registry->get<CameraComponent>(e);
    c.shake_mag = mag;
    c.shake_time = time;
    return 0;
}

int l_set_zoom(lua_State* L)
{
    float z = (float)luaL_checknumber(L, 1);
    auto e = FindByTag("camera");
    if (e == entt::null || !g_registry || !g_registry->has<CameraComponent>(e))
        return 0;
    g_registry->get<CameraComponent>(e).zoom = z;
    return 0;
}

int l_get_view_half(lua_State* L)
{
    float zoom = 1.f;
    auto e = FindByTag("camera");
    if (e != entt::null && g_registry && g_registry->has<CameraComponent>(e))
        zoom = g_registry->get<CameraComponent>(e).zoom;
    if (zoom < 0.01f)
        zoom = 0.01f;
    auto& gl = GlContext::GetInstance();
    const float half_w = (gl.Width() / kWorldPixels) * 0.5f / zoom;
    const float half_h = (gl.Height() / kWorldPixels) * 0.5f / zoom;
    lua_pushnumber(L, half_w);
    lua_pushnumber(L, half_h);
    return 2;
}

int l_spawn_deco(lua_State* L)
{
    // cosmetic filled quad, no physics
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    int r = (int)luaL_optinteger(L, 6, 80);
    int g = (int)luaL_optinteger(L, 7, 90);
    int b = (int)luaL_optinteger(L, 8, 120);
    int a = (int)luaL_optinteger(L, 9, 160);

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }
    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<ColorComponent>();
    e.AddComponent<DebugDrawComponent>();
    e.AddComponent<BoxShapeComponent>();
    e.GetComponent<TransformComponent>().x = x;
    e.GetComponent<TransformComponent>().y = y;
    e.GetComponent<ColorComponent>().primary = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    e.GetComponent<BoxShapeComponent>().width = w;
    e.GetComponent<BoxShapeComponent>().height = h;
    e.GetComponent<DebugDrawComponent>().filled = true;
    e.GetComponent<DebugDrawComponent>().outline = false;
    e.GetComponent<DebugDrawComponent>().sort = -50;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_spawn_sprite(lua_State* L)
{
    // static image, no physics
    const char* tag = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    float ppu = (float)luaL_optnumber(L, 5, 1.0);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushnil(L);
        return 1;
    }
    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<SpriteComponent>();
    auto& t = e.GetComponent<TransformComponent>();
    t.x = x;
    t.y = y;
    t.ix = x;
    t.iy = y;
    auto& spr = e.GetComponent<SpriteComponent>();
    spr.path_owned = path ? path : "";
    spr.image_location = spr.path_owned.empty() ? nullptr : spr.path_owned.c_str();
    spr.ppu = ppu;
    spr.sort_key = -1000;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_set_sprite_ppu(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float ppu = (float)luaL_checknumber(L, 2);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<SpriteComponent>(e))
        return 0;
    g_registry->get<SpriteComponent>(e).ppu = ppu;
    return 0;
}

int l_set_sprite_path(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<SpriteComponent>(e))
        return 0;
    auto& spr = g_registry->get<SpriteComponent>(e);
    spr.path_owned = path ? path : "";
    spr.image_location = spr.path_owned.empty() ? nullptr : spr.path_owned.c_str();
    return 0;
}

int l_ensure_world_atlas(lua_State* L)
{
    (void)L;
    WorldAtlas::GetInstance().EnsureContentPaths();
    return 0;
}

int l_spawn_sprite_grid(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    int cols = (int)luaL_checkinteger(L, 4);
    int rows = (int)luaL_checkinteger(L, 5);
    const char* body = luaL_checkstring(L, 6);
    const char* top = nullptr;
    float ppu = 2.f;
    float cell = 1.f;
    int arg = 7;
    if (lua_type(L, arg) == LUA_TSTRING)
    {
        top = lua_tostring(L, arg);
        ++arg;
    }
    else if (lua_type(L, arg) == LUA_TNIL)
        ++arg;
    if (lua_isnumber(L, arg))
    {
        ppu = (float)lua_tonumber(L, arg);
        ++arg;
    }
    if (lua_isnumber(L, arg))
        cell = (float)lua_tonumber(L, arg);

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || cols <= 0 || rows <= 0 || !body)
    {
        lua_pushnil(L);
        return 1;
    }
    WorldAtlas::GetInstance().EnsureContentPaths();

    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<SpriteGridComponent>();
    auto& t = e.GetComponent<TransformComponent>();
    t.x = x;
    t.y = y;
    t.ix = x;
    t.iy = y;
    auto& g = e.GetComponent<SpriteGridComponent>();
    g.body_path_owned = body;
    g.body_path = g.body_path_owned.c_str();
    if (top && top[0])
    {
        g.top_path_owned = top;
        g.top_path = g.top_path_owned.c_str();
    }
    g.cols = cols;
    g.rows = rows;
    g.ppu = ppu;
    g.cell = cell > 0.01f ? cell : 1.f;
    g.sort_key = -1000;
    g.visible = true;
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_set_sprite_grid(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int cols = (int)luaL_checkinteger(L, 2);
    int rows = (int)luaL_checkinteger(L, 3);
    const char* body = luaL_checkstring(L, 4);
    const char* top = nullptr;
    float ppu = 2.f;
    float cell = 1.f;
    int arg = 5;
    if (lua_type(L, arg) == LUA_TSTRING)
    {
        top = lua_tostring(L, arg);
        ++arg;
    }
    else if (lua_type(L, arg) == LUA_TNIL)
        ++arg;
    if (lua_isnumber(L, arg))
    {
        ppu = (float)lua_tonumber(L, arg);
        ++arg;
    }
    if (lua_isnumber(L, arg))
        cell = (float)lua_tonumber(L, arg);

    if (!g_registry || cols <= 0 || rows <= 0 || !body)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    WorldAtlas::GetInstance().EnsureContentPaths();
    if (!g_registry->has<SpriteGridComponent>(e))
        g_registry->emplace<SpriteGridComponent>(e);
    if (g_registry->has<SpriteComponent>(e))
    {
        // Prefer grid; silence any leftover single-sprite on same entity.
        g_registry->get<SpriteComponent>(e).image_location = nullptr;
    }
    auto& g = g_registry->get<SpriteGridComponent>(e);
    g.body_path_owned = body;
    g.body_path = g.body_path_owned.c_str();
    if (top && top[0])
    {
        g.top_path_owned = top;
        g.top_path = g.top_path_owned.c_str();
    }
    else
    {
        g.top_path_owned.clear();
        g.top_path = nullptr;
    }
    g.cols = cols;
    g.rows = rows;
    g.ppu = ppu;
    g.cell = cell > 0.01f ? cell : 1.f;
    g.visible = true;
    g.sheet_cols = 0;
    g.sheet_rows = 0;
    g.frames.clear();
    return 0;
}

int l_set_sprite_grid_frames(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int sheet_cols = (int)luaL_checkinteger(L, 2);
    int sheet_rows = (int)luaL_checkinteger(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    if (!g_registry || sheet_cols <= 0 || sheet_rows <= 0)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<SpriteGridComponent>(e))
        return 0;
    auto& g = g_registry->get<SpriteGridComponent>(e);
    g.sheet_cols = sheet_cols;
    g.sheet_rows = sheet_rows;
    const int n = g.cols * g.rows;
    g.frames.assign(n > 0 ? (size_t)n : 0, -1);
    const int len = lua_objlen(L, 4);
    for (int i = 0; i < n && i < len; ++i)
    {
        lua_rawgeti(L, 4, i + 1);
        if (lua_isnumber(L, -1))
            g.frames[(size_t)i] = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    return 0;
}

int l_set_sheet_anim(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int columns = (int)luaL_checkinteger(L, 3);
    float ppu = (float)luaL_optnumber(L, 4, 2.0);
    int rows = (int)luaL_optinteger(L, 5, 1);
    if (!g_registry || columns <= 0 || rows <= 0)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    if (!g_registry->has<SpriteSheetComponent>(e))
        g_registry->emplace<SpriteSheetComponent>(e);
    if (!g_registry->has<AnimationComponent>(e))
        g_registry->emplace<AnimationComponent>(e);
    auto& sheet = g_registry->get<SpriteSheetComponent>(e);
    auto& anim = g_registry->get<AnimationComponent>(e);
    sheet.path_owned = path ? path : "";
    sheet.image_location = sheet.path_owned.empty() ? nullptr : sheet.path_owned.c_str();
    sheet.columns = columns;
    sheet.rows = rows;
    sheet.ppu = ppu;
    sheet.visible = true;
    sheet.cut_image = nullptr;
    sheet.cut_rows = -1;
    sheet.cut_columns = -1;
    anim.enabled = true;
    anim.frame_start = 0;
    anim.frame_end = -1;
    anim.current_frame = 0.f;
    return 0;
}

int l_set_sheet_column_anim(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int columns = (int)luaL_checkinteger(L, 3);
    int rows = (int)luaL_checkinteger(L, 4);
    int column = (int)luaL_checkinteger(L, 5);
    float ppu = (float)luaL_optnumber(L, 6, 2.0);
    if (!g_registry || !path || columns <= 0 || rows <= 0 || column < 0 || column >= columns)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    if (g_registry->has<SpriteComponent>(e))
    {
        auto& spr = g_registry->get<SpriteComponent>(e);
        spr.image_location = nullptr;
        spr.path_owned.clear();
    }
    if (!g_registry->has<SpriteSheetComponent>(e))
        g_registry->emplace<SpriteSheetComponent>(e);
    if (!g_registry->has<AnimationComponent>(e))
        g_registry->emplace<AnimationComponent>(e);

    auto& sheet = g_registry->get<SpriteSheetComponent>(e);
    auto& anim = g_registry->get<AnimationComponent>(e);
    sheet.path_owned = path;
    sheet.image_location = sheet.path_owned.c_str();
    sheet.ppu = ppu;
    sheet.visible = true;
    sheet.align_feet = false; // tile center ↔ tile center
    sheet.flip_horizontal = false;
    sheet.flip_vertical = false;

    auto& tex = AssetCache::GetInstance().GetTexture(path);
    sheet.sprites.clear();
    if (tex.surface && tex.width > 0 && tex.height > 0)
    {
        const int tw = tex.width / columns;
        const int th = tex.height / rows;
        if (tw > 0 && th > 0)
        {
            for (int y = 0; y < rows; ++y)
                sheet.sprites.push_back(SDL_Rect{column * tw, y * th, tw, th});
        }
    }
    // Present as a 1×N strip so CutSprites will not rebuild over our rects.
    sheet.columns = 1;
    sheet.rows = rows;
    sheet.cut_image = sheet.image_location;
    sheet.cut_columns = 1;
    sheet.cut_rows = rows;

    anim.enabled = true;
    anim.frame_start = 0;
    anim.frame_end = rows > 0 ? (rows - 1) : 0;
    anim.current_frame = 0.f;
    return 0;
}

int l_play_sheet_anim(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float rate = (float)luaL_checknumber(L, 2);
    bool looping = lua_toboolean(L, 3) != 0;
    int start_frame = (int)luaL_checkinteger(L, 4);
    int end_frame = (int)luaL_checkinteger(L, 5);
    const char* state = luaL_optstring(L, 6, nullptr);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<AnimationComponent>(e))
        return 0;
    auto& anim = g_registry->get<AnimationComponent>(e);
    anim.frame_rate = rate;
    anim.looping = looping;
    anim.frame_start = start_frame;
    anim.frame_end = end_frame;
    anim.play = true;
    anim.enabled = true;
    bool restart = anim.current_frame < (float)start_frame
        || anim.current_frame > (float)end_frame + 0.999f;
    if (state)
    {
        if (anim.state != state)
            restart = true;
        anim.state = state;
    }
    if (restart)
        anim.current_frame = (float)start_frame;
    return 0;
}

int l_get_sheet_anim_frame(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry)
    {
        lua_pushnil(L);
        return 1;
    }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<AnimationComponent>(e))
    {
        lua_pushnil(L);
        return 1;
    }
    const auto& anim = g_registry->get<AnimationComponent>(e);
    lua_pushnumber(L, anim.current_frame);
    lua_pushboolean(L, anim.play ? 1 : 0);
    if (!anim.state.empty())
        lua_pushstring(L, anim.state.c_str());
    else
        lua_pushnil(L);
    lua_pushinteger(L, anim.frame_start);
    lua_pushinteger(L, anim.frame_end);
    lua_pushnumber(L, anim.frame_rate);
    lua_pushboolean(L, anim.looping ? 1 : 0);
    return 7;
}

int l_pause_sheet_anim(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<AnimationComponent>(e))
        return 0;
    g_registry->get<AnimationComponent>(e).play = false;
    return 0;
}

int l_resume_sheet_anim(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<AnimationComponent>(e))
        return 0;
    g_registry->get<AnimationComponent>(e).play = true;
    return 0;
}

int l_set_sheet_anim_frame(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float frame = (float)luaL_checknumber(L, 2);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<AnimationComponent>(e))
        return 0;
    auto& anim = g_registry->get<AnimationComponent>(e);
    int start = anim.frame_start;
    if (start < 0)
        start = 0;
    int end = anim.frame_end;
    if (g_registry->has<SpriteSheetComponent>(e))
    {
        const auto& sheet = g_registry->get<SpriteSheetComponent>(e);
        const int n = (int)sheet.sprites.size();
        if (end < 0)
            end = n > 0 ? (n - 1) : 0;
        if (end >= n)
            end = n > 0 ? (n - 1) : 0;
    }
    else if (end < 0)
        end = start;
    if (frame < (float)start)
        frame = (float)start;
    if (frame > (float)end + 0.999f)
        frame = (float)end;
    anim.current_frame = frame;
    return 0;
}

int l_set_sheet_flip(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool flip_h = lua_toboolean(L, 2) != 0;
    bool flip_v = lua_toboolean(L, 3) != 0;
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<SpriteSheetComponent>(e))
        return 0;
    auto& sheet = g_registry->get<SpriteSheetComponent>(e);
    sheet.flip_horizontal = flip_h;
    sheet.flip_vertical = flip_v;
    return 0;
}

int l_set_sheet_angle(lua_State* L)
{
    // Visual-only rotation (radians). Does not rotate the rigid body.
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float a = (float)luaL_checknumber(L, 2);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<SpriteSheetComponent>(e))
        return 0;
    g_registry->get<SpriteSheetComponent>(e).angle_offset = a;
    return 0;
}

int l_set_sheet_visible(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool vis = lua_toboolean(L, 2) != 0;
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    if (g_registry->has<SpriteSheetComponent>(e))
        g_registry->get<SpriteSheetComponent>(e).visible = vis;
    if (g_registry->has<AnimationComponent>(e))
        g_registry->get<AnimationComponent>(e).enabled = vis;
    return 0;
}

int l_set_sheet_palette(lua_State* L)
{
    // empty/nil = raw RGBA sheet
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const char* path = lua_tostring(L, 2);
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    if (!g_registry->has<SpriteSheetComponent>(e))
        g_registry->emplace<SpriteSheetComponent>(e);
    auto& sheet = g_registry->get<SpriteSheetComponent>(e);
    if (!path || !path[0])
    {
        sheet.palette_owned.clear();
        sheet.palette_location = nullptr;
        return 0;
    }
    sheet.palette_owned = path;
    sheet.palette_location = sheet.palette_owned.c_str();
    return 0;
}

int l_set_box_draw(lua_State* L)
{
    // filled debug body
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool show = lua_toboolean(L, 2) != 0;
    if (!g_registry)
        return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e))
        return 0;
    if (show)
    {
        if (!g_registry->has<DebugDrawComponent>(e))
            g_registry->emplace<DebugDrawComponent>(e);
        auto& dbg = g_registry->get<DebugDrawComponent>(e);
        dbg.filled = true;
        dbg.outline = false;
        if (dbg.sort == 0)
            dbg.sort = 15;
    }
    else if (g_registry->has<DebugDrawComponent>(e))
    {
        auto& dbg = g_registry->get<DebugDrawComponent>(e);
        dbg.filled = false;
        dbg.outline = false;
    }
    return 0;
}

int l_texture_ok(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, AssetCache::GetInstance().HasUsableTexture(path) ? 1 : 0);
    return 1;
}

void RegisterRenderBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(set_color, l_set_color);
    BIND(set_clear_color, l_set_clear_color);
    BIND(texture_ok, l_texture_ok);
    BIND(set_world_textures, l_set_world_textures);
    BIND(get_world_textures, l_get_world_textures);
    BIND(set_sort_band, l_set_sort_band);
    BIND(set_emissive, l_set_emissive);
    BIND(set_light_volume, l_set_light_volume);
    BIND(set_light_volume_size, l_set_light_volume_size);
    BIND(set_light_shaft, l_set_light_shaft);
    BIND(set_light_fill, l_set_light_fill);
    BIND(get_light_fill, l_get_light_fill);
    BIND(rebuild_occluders, l_rebuild_occluders);
    BIND(set_debug_occluders, l_set_debug_occluders);
    BIND(spawn_point_light, l_spawn_point_light);
    BIND(set_point_light, l_set_point_light);
    BIND(spawn_light_volume, l_spawn_light_volume);
    BIND(spawn_light_shaft, l_spawn_light_shaft);
    BIND(spawn_aperture, l_spawn_aperture);
    BIND(shake_camera, l_shake_camera);
    BIND(set_zoom, l_set_zoom);
    BIND(get_view_half, l_get_view_half);
    BIND(spawn_deco, l_spawn_deco);
    BIND(spawn_sprite, l_spawn_sprite);
    BIND(set_sprite_ppu, l_set_sprite_ppu);
    BIND(set_sprite_path, l_set_sprite_path);
    BIND(ensure_world_atlas, l_ensure_world_atlas);
    BIND(spawn_sprite_grid, l_spawn_sprite_grid);
    BIND(set_sprite_grid, l_set_sprite_grid);
    BIND(set_sprite_grid_frames, l_set_sprite_grid_frames);
    BIND(set_sheet_anim, l_set_sheet_anim);
    BIND(set_sheet_column_anim, l_set_sheet_column_anim);
    BIND(play_sheet_anim, l_play_sheet_anim);
    BIND(get_sheet_anim_frame, l_get_sheet_anim_frame);
    BIND(pause_sheet_anim, l_pause_sheet_anim);
    BIND(resume_sheet_anim, l_resume_sheet_anim);
    BIND(set_sheet_anim_frame, l_set_sheet_anim_frame);
    BIND(set_sheet_flip, l_set_sheet_flip);
    BIND(set_sheet_angle, l_set_sheet_angle);
    BIND(set_sheet_visible, l_set_sheet_visible);
    BIND(set_sheet_palette, l_set_sheet_palette);
    BIND(set_box_draw, l_set_box_draw);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterRenderBinds(lua_State* L)
{
    RegisterRenderBindsImpl(L);
}

#endif // FW_HAS_LUAU
