#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Core/gameSettings.h"
#include "Core/log.h"
#include "Core/paths.h"
#include "Scene/entity.h"
#include "Scene/sceneHandler.h"
#include "box2d.h"
#include "lua.h"
#include "lualib.h"
#include <cmath>
#include <cstdlib>
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
using ScriptInternal::g_ui_default_font;
using ScriptInternal::UiPaintDefaults;

namespace {

int l_settings_get_bloom(lua_State* L)
{
    lua_pushinteger(L, GameSettings::GetInstance().BloomQuality());
    return 1;
}

int l_settings_set_bloom(lua_State* L)
{
    GameSettings::GetInstance().SetBloomQuality((int)luaL_checkinteger(L, 1));
    return 0;
}

int l_settings_get_vignette(lua_State* L)
{
    lua_pushnumber(L, GameSettings::GetInstance().Vignette());
    return 1;
}

int l_settings_set_vignette(lua_State* L)
{
    GameSettings::GetInstance().SetVignette((float)luaL_checknumber(L, 1));
    return 0;
}

int l_settings_get_reduce_flash(lua_State* L)
{
    lua_pushboolean(L, GameSettings::GetInstance().ReduceFlash() ? 1 : 0);
    return 1;
}

int l_settings_set_reduce_flash(lua_State* L)
{
    GameSettings::GetInstance().SetReduceFlash(lua_toboolean(L, 1) != 0);
    return 0;
}

int l_settings_get_volume(lua_State* L)
{
    const char* bus = luaL_checkstring(L, 1);
    auto& s = GameSettings::GetInstance();
    float v = s.Sfx();
    if (std::strcmp(bus, "master") == 0)
        v = s.Master();
    else if (std::strcmp(bus, "music") == 0)
        v = s.Music();
    else if (std::strcmp(bus, "ui") == 0)
        v = s.Ui();
    lua_pushnumber(L, v);
    return 1;
}

int l_settings_set_volume(lua_State* L)
{
    const char* bus = luaL_checkstring(L, 1);
    float v = (float)luaL_checknumber(L, 2);
    auto& s = GameSettings::GetInstance();
    if (std::strcmp(bus, "master") == 0)
        s.SetMaster(v);
    else if (std::strcmp(bus, "music") == 0)
        s.SetMusic(v);
    else if (std::strcmp(bus, "ui") == 0)
        s.SetUi(v);
    else
        s.SetSfx(v);
    return 0;
}

int l_set_text(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);
    auto e = FindByTag(tag);
    if (e == entt::null || !g_registry || !g_registry->has<TextComponent>(e))
        return 0;
    g_registry->get<TextComponent>(e).text = text;
    return 0;
}

int l_set_font(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int size = (int)luaL_optinteger(L, 3, 0);
    auto e = FindByTag(tag);
    if (e == entt::null || !g_registry || !g_registry->has<FontComponent>(e))
        return 0;
    static std::unordered_map<std::string, std::string> s_entity_fonts;
    s_entity_fonts[tag] = path;
    auto& fontc = g_registry->get<FontComponent>(e);
    fontc.font_location = s_entity_fonts[tag].c_str();
    if (size > 0)
        fontc.size = size;
    if (g_registry->has<TextComponent>(e))
    {
        auto& text = g_registry->get<TextComponent>(e);
        text.cached_tex = 0;
        text.cache_key.clear();
    }
    return 0;
}

int l_get_text(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    auto e = FindByTag(tag);
    if (e == entt::null || !g_registry || !g_registry->has<TextComponent>(e))
    {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, g_registry->get<TextComponent>(e).text.c_str());
    return 1;
}

int l_emit_burst(lua_State* L)
{
    // optional x,y = world spawn (else entity transform)
    int count = (int)luaL_checkinteger(L, 2);
    float r = (float)luaL_optnumber(L, 3, 1.0);
    float g = (float)luaL_optnumber(L, 4, 1.0);
    float b = (float)luaL_optnumber(L, 5, 1.0);
    bool has_pos = lua_gettop(L) >= 7;
    float sx = has_pos ? (float)luaL_checknumber(L, 6) : 0.f;
    float sy = has_pos ? (float)luaL_checknumber(L, 7) : 0.f;

    // lua_isstring is true for numbers, so ids must be excluded explicitly.
    entt::entity e = entt::null;
    if (lua_isstring(L, 1) && !lua_isnumber(L, 1))
        e = FindByTag(lua_tostring(L, 1));
    else
        e = EntityFromId((uint32_t)luaL_checkinteger(L, 1));

    if (e == entt::null || !g_registry) return 0;
    if (!g_registry->has<ParticleEmitterComponent>(e))
        return 0;
    if (!g_registry->has<ParticleBufferComponent>(e))
        g_registry->emplace<ParticleBufferComponent>(e);
    auto& em = g_registry->get<ParticleEmitterComponent>(e);
    auto& buf = g_registry->get<ParticleBufferComponent>(e);
    // Tint only for this burst. Continuous emitters (foot dust) must keep
    // their own RGB - otherwise one-shot FX permanently recolor trails.
    const float prev_r = em.r, prev_g = em.g, prev_b = em.b;
    em.r = r; em.g = g; em.b = b;

    float ox = sx, oy = sy;
    if (!has_pos && g_registry->has<TransformComponent>(e))
    {
        ox = g_registry->get<TransformComponent>(e).x;
        oy = g_registry->get<TransformComponent>(e).y;
    }

    // Spawn immediately so destroy()/teleport same frame still shows FX
    for (int i = 0; i < count; ++i)
    {
        float ang = em.angle + ((rand() % 1000) / 1000.f) * em.spread - em.spread * 0.5f;
        Particle p;
        p.x = ox;
        p.y = oy;
        p.vx = std::cos(ang) * em.speed;
        p.vy = std::sin(ang) * em.speed;
        p.life = em.life;
        p.max_life = em.life;
        p.size = em.size;
        buf.particles.push_back(p);
    }
    em.r = prev_r; em.g = prev_g; em.b = prev_b;
    return 0;
}

int l_emit_trail(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float r = (float)luaL_optnumber(L, 3, 0.5);
    float g = (float)luaL_optnumber(L, 4, 0.85);
    float b = (float)luaL_optnumber(L, 5, 1.0);
    int count = (int)luaL_optinteger(L, 6, 6);
    if (count < 1) count = 1;
    if (count > 24) count = 24;

    if (!g_registry) return 0;
    auto e = FindByTag("fx_pop");
    if (e == entt::null) return 0;
    if (!g_registry->has<ParticleEmitterComponent>(e))
        return 0;
    if (!g_registry->has<ParticleBufferComponent>(e))
        g_registry->emplace<ParticleBufferComponent>(e);
    auto& em = g_registry->get<ParticleEmitterComponent>(e);
    auto& buf = g_registry->get<ParticleBufferComponent>(e);
    const float prev_r = em.r, prev_g = em.g, prev_b = em.b;
    em.r = r; em.g = g; em.b = b;

    for (int i = 0; i < count; ++i)
    {
        float ang = ((rand() % 1000) / 1000.f) * 6.28318f;
        float spd = 0.15f + ((rand() % 1000) / 1000.f) * 0.55f;
        Particle p;
        p.x = x + (((rand() % 1000) / 1000.f) - 0.5f) * 0.08f;
        p.y = y + (((rand() % 1000) / 1000.f) - 0.5f) * 0.08f;
        p.vx = std::cos(ang) * spd;
        p.vy = std::sin(ang) * spd;
        p.life = 0.22f + ((rand() % 1000) / 1000.f) * 0.18f;
        p.max_life = p.life;
        p.size = 0.08f + ((rand() % 1000) / 1000.f) * 0.06f;
        buf.particles.push_back(p);
    }
    em.r = prev_r; em.g = prev_g; em.b = prev_b;
    return 0;
}

int l_emit_splash(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float r = (float)luaL_optnumber(L, 3, 0.55);
    float g = (float)luaL_optnumber(L, 4, 0.9);
    float b = (float)luaL_optnumber(L, 5, 1.0);
    int count = (int)luaL_optinteger(L, 6, 10);
    if (count < 1) count = 1;
    if (count > 28) count = 28;

    if (!g_registry) return 0;
    auto e = FindByTag("fx_pop");
    if (e == entt::null) return 0;
    if (!g_registry->has<ParticleEmitterComponent>(e))
        return 0;
    if (!g_registry->has<ParticleBufferComponent>(e))
        g_registry->emplace<ParticleBufferComponent>(e);
    auto& em = g_registry->get<ParticleEmitterComponent>(e);
    auto& buf = g_registry->get<ParticleBufferComponent>(e);
    const float prev_r = em.r, prev_g = em.g, prev_b = em.b;
    em.r = r; em.g = g; em.b = b;

    for (int i = 0; i < count; ++i)
    {
        // Angle in [-pi, 0]: vy = sin(ang) <= 0 → sprays upward/out of the pool
        const float ang = -3.14159265f * ((rand() % 1000) / 1000.f);
        const float spd = 1.2f + ((rand() % 1000) / 1000.f) * 3.2f;
        Particle p;
        p.x = x + (((rand() % 1000) / 1000.f) - 0.5f) * 0.35f;
        p.y = y + (((rand() % 1000) / 1000.f) - 0.5f) * 0.04f;
        p.vx = std::cos(ang) * spd;
        p.vy = std::sin(ang) * spd;
        p.life = 0.28f + ((rand() % 1000) / 1000.f) * 0.22f;
        p.max_life = p.life;
        p.size = 0.06f + ((rand() % 1000) / 1000.f) * 0.05f;
        buf.particles.push_back(p);
    }
    em.r = prev_r; em.g = prev_g; em.b = prev_b;
    return 0;
}

int l_emit_mote(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float vx = (float)luaL_checknumber(L, 3);
    float vy = (float)luaL_checknumber(L, 4);
    float r = (float)luaL_optnumber(L, 5, 0.65);
    float g = (float)luaL_optnumber(L, 6, 0.25);
    float b = (float)luaL_optnumber(L, 7, 0.95);
    float size = (float)luaL_optnumber(L, 8, 0.05);
    float life = (float)luaL_optnumber(L, 9, 0.45);
    int count = (int)luaL_optinteger(L, 10, 1);
    if (count < 1) count = 1;
    if (count > 12) count = 12;

    if (!g_registry) return 0;
    auto e = FindByTag("fx_pop");
    if (e == entt::null) return 0;
    if (!g_registry->has<ParticleEmitterComponent>(e))
        return 0;
    if (!g_registry->has<ParticleBufferComponent>(e))
        g_registry->emplace<ParticleBufferComponent>(e);
    auto& em = g_registry->get<ParticleEmitterComponent>(e);
    auto& buf = g_registry->get<ParticleBufferComponent>(e);
    const float prev_r = em.r, prev_g = em.g, prev_b = em.b;
    em.r = r; em.g = g; em.b = b;

    for (int i = 0; i < count; ++i)
    {
        Particle p;
        p.x = x + (((rand() % 1000) / 1000.f) - 0.5f) * 0.04f;
        p.y = y + (((rand() % 1000) / 1000.f) - 0.5f) * 0.04f;
        p.vx = vx + (((rand() % 1000) / 1000.f) - 0.5f) * 0.12f;
        p.vy = vy + (((rand() % 1000) / 1000.f) - 0.5f) * 0.12f;
        p.life = life * (0.75f + ((rand() % 1000) / 1000.f) * 0.5f);
        p.max_life = p.life;
        p.size = size * (0.7f + ((rand() % 1000) / 1000.f) * 0.5f);
        buf.particles.push_back(p);
    }
    em.r = prev_r; em.g = prev_g; em.b = prev_b;
    return 0;
}

int l_set_emitting(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool on = lua_toboolean(L, 2) != 0;
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<ParticleEmitterComponent>(e)) return 0;
    g_registry->get<ParticleEmitterComponent>(e).emitting = on;
    return 0;
}

int l_destroy_hint(lua_State* L)
{
    // Soft-destroy: hide + disable (keep entity/tag for scripts)
    if (!g_registry) return 0;
    auto e = ResolveEntityArg(L, 1);
    if (!g_registry->valid(e)) return 0;
    if (g_registry->has<TransformComponent>(e))
    {
        auto& t = g_registry->get<TransformComponent>(e);
        t.x = -9999.f; t.y = -9999.f; t.s = 0.f;
    }
    if (g_registry->has<RigidBody2DComponent>(e) && g_registry->get<RigidBody2DComponent>(e).body)
    {
        auto* body = g_registry->get<RigidBody2DComponent>(e).body;
        body->SetTransform(b2Vec2(-9999.f, -9999.f), 0);
        body->SetLinearVelocity(b2Vec2(0, 0));
        body->SetEnabled(false);
    }
    if (g_registry->has<DebugDrawComponent>(e))
        g_registry->remove<DebugDrawComponent>(e);
    if (g_registry->has<LightVolumeComponent>(e))
        g_registry->remove<LightVolumeComponent>(e);
    if (g_registry->has<LightShaftComponent>(e))
        g_registry->remove<LightShaftComponent>(e);
    if (g_registry->has<EmissiveComponent>(e))
        g_registry->remove<EmissiveComponent>(e);
    if (g_registry->has<ApertureComponent>(e))
        g_registry->get<ApertureComponent>(e).active = false;
    if (g_registry->has<ColorComponent>(e))
        g_registry->get<ColorComponent>(e).primary.a = 0;
    if (g_registry->has<BoxShapeComponent>(e))
    {
        g_registry->get<BoxShapeComponent>(e).width = 0.01f;
        g_registry->get<BoxShapeComponent>(e).height = 0.01f;
    }
    if (g_registry->has<ParticleBufferComponent>(e))
        g_registry->get<ParticleBufferComponent>(e).particles.clear();
    if (g_registry->has<ParticleEmitterComponent>(e))
        g_registry->get<ParticleEmitterComponent>(e).emitting = false;
    // keep em.burst so a same-frame emit_burst still spawns in Particles::Run
    if (g_registry->has<Collider2DComponent>(e))
    {
        auto& col = g_registry->get<Collider2DComponent>(e);
        col.sensor = true;
        if (col.fixture)
        {
            col.fixture->SetSensor(true);
            b2Filter f = col.fixture->GetFilterData();
            f.maskBits = 0;
            col.fixture->SetFilterData(f);
        }
    }
    return 0;
}

int l_despawn(lua_State* L)
{
    // Hard remove (unlike destroy, which parks). For leftover art / one-shots.
    if (!g_registry)
        return 0;
    auto e = ResolveEntityArg(L, 1);
    if (!g_registry->valid(e))
        return 0;
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
        return 0;
    scene->DestroyEntity(e);
    return 0;
}

int l_restore_hint(lua_State* L)
{
    if (!g_registry) return 0;
    auto e = ResolveEntityArg(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_optnumber(L, 4, 0.45);
    float h = (float)luaL_optnumber(L, 5, 0.45);
    if (!g_registry->valid(e)) return 0;
    if (g_registry->has<TransformComponent>(e))
    {
        auto& t = g_registry->get<TransformComponent>(e);
        t.x = x; t.y = y; t.s = 1.f;
    }
    if (g_registry->has<BoxShapeComponent>(e))
    {
        g_registry->get<BoxShapeComponent>(e).width = w;
        g_registry->get<BoxShapeComponent>(e).height = h;
    }
    if (g_registry->has<ColorComponent>(e))
        g_registry->get<ColorComponent>(e).primary.a = 255;
    if (!g_registry->has<DebugDrawComponent>(e))
        g_registry->emplace<DebugDrawComponent>(e);
    if (g_registry->has<RigidBody2DComponent>(e) && g_registry->get<RigidBody2DComponent>(e).body)
    {
        auto* body = g_registry->get<RigidBody2DComponent>(e).body;
        body->SetEnabled(true);
        body->SetTransform(b2Vec2(x, y), 0);
        body->SetLinearVelocity(b2Vec2(0, 0));
    }
    if (g_registry->has<Collider2DComponent>(e) && g_registry->get<Collider2DComponent>(e).fixture)
    {
        auto& col = g_registry->get<Collider2DComponent>(e);
        col.fixture->SetSensor(col.sensor);
        b2Filter f = col.fixture->GetFilterData();
        f.categoryBits = col.category_bits;
        f.maskBits = col.mask_bits;
        col.fixture->SetFilterData(f);
    }
    return 0;
}

int l_spawn_box(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    int r = (int)luaL_optinteger(L, 6, 200);
    int g = (int)luaL_optinteger(L, 7, 200);
    int b = (int)luaL_optinteger(L, 8, 200);
    bool dynamic = lua_toboolean(L, 9) != 0;

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene) { lua_pushnil(L); return 1; }

    Entity e = scene->CreateEntity(tag);
    e.AddComponent<TransformComponent>();
    e.AddComponent<ColorComponent>();
    e.AddComponent<DebugDrawComponent>();
    e.AddComponent<RigidBody2DComponent>();
    e.AddComponent<BoxShapeComponent>();
    e.AddComponent<MaterialComponent>();
    e.AddComponent<Collider2DComponent>();
    e.AddComponent<CollisionComponent>();

    e.GetComponent<TransformComponent>().x = x;
    e.GetComponent<TransformComponent>().y = y;
    e.GetComponent<ColorComponent>().primary = {(Uint8)r, (Uint8)g, (Uint8)b, 255};
    auto& rb = e.GetComponent<RigidBody2DComponent>();
    rb.type = dynamic ? b2_dynamicBody : b2_staticBody;
    rb.gravity_scale = dynamic ? 1.f : 0.f;
    e.GetComponent<BoxShapeComponent>().width = w;
    e.GetComponent<BoxShapeComponent>().height = h;
    e.GetComponent<Collider2DComponent>().shape = Shape::box;

    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_ui_set_default_font(lua_State* L)
{
    const char* path = luaL_optstring(L, 1, nullptr);
    g_ui_default_font = path ? path : "";
    return 0;
}

int l_ui_button(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    int w = (int)luaL_checkinteger(L, 4);
    int h = (int)luaL_checkinteger(L, 5);
    const char* text = luaL_optstring(L, 6, "Button");

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushnil(L);
        return 1;
    }

    Entity existing = scene->GetEntity(tag);
    Entity e = existing;
    if (e.GetHandle() == entt::null)
    {
        e = scene->CreateEntity(tag);
        e.AddComponent<ButtonComponent>();
    }
    else if (!e.HasComponent<ButtonComponent>())
    {
        e.AddComponent<ButtonComponent>();
    }

    auto& b = e.GetComponent<ButtonComponent>();
    UiPaintDefaults(b);
    b.text = text ? text : "Button";
    b.rect.w = w;
    b.rect.h = h;
    Rect::Update(b.rect, x, y);
    lua_pushinteger(L, (int)(uint32_t)e.GetHandle());
    return 1;
}

int l_ui_set(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    const char* field = luaL_checkstring(L, 2);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
        return 0;
    Entity e = scene->GetEntity(tag);
    if (e.GetHandle() == entt::null || !e.HasComponent<ButtonComponent>())
        return 0;
    auto& b = e.GetComponent<ButtonComponent>();
    if (std::strcmp(field, "text") == 0)
        b.text = luaL_checkstring(L, 3);
    else if (std::strcmp(field, "active") == 0)
        b.active = lua_toboolean(L, 3) != 0;
    else if (std::strcmp(field, "x") == 0)
        Rect::Update(b.rect, (float)luaL_checknumber(L, 3), b.rect.y);
    else if (std::strcmp(field, "y") == 0)
        Rect::Update(b.rect, b.rect.x, (float)luaL_checknumber(L, 3));
    else if (std::strcmp(field, "xy") == 0)
        Rect::Update(b.rect, (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
    else if (std::strcmp(field, "font") == 0)
    {
        static std::unordered_map<std::string, std::string> s_font_paths;
        const char* path = luaL_checkstring(L, 3);
        s_font_paths[tag] = path;
        b.font_location = s_font_paths[tag].c_str();
    }
    else if (std::strcmp(field, "font_size") == 0)
        b.font_size = (int)luaL_checkinteger(L, 3);
    else if (std::strcmp(field, "outline") == 0)
        b.outline = lua_toboolean(L, 3) != 0;
    else if (std::strcmp(field, "text_align") == 0)
    {
        const char* a = luaL_checkstring(L, 3);
        b.text_align = (std::strcmp(a, "left") == 0) ? 1 : 0;
    }
    else if (std::strcmp(field, "fill") == 0)
    {
        // fill r g b a - idle/hover/pressed share the same fill (dim overlays).
        const Uint8 r = (Uint8)luaL_checkinteger(L, 3);
        const Uint8 g = (Uint8)luaL_checkinteger(L, 4);
        const Uint8 bcol = (Uint8)luaL_checkinteger(L, 5);
        const Uint8 a = (Uint8)luaL_optinteger(L, 6, 255);
        b.color.primary = {r, g, bcol, a};
        b.hover_color.primary = {r, g, bcol, a};
        b.selected_color.primary = {r, g, bcol, a};
    }
    else if (std::strcmp(field, "size") == 0)
    {
        b.rect.w = (int)luaL_checkinteger(L, 3);
        b.rect.h = (int)luaL_checkinteger(L, 4);
        Rect::Update(b.rect, b.rect.x, b.rect.y);
    }
    else if (std::strcmp(field, "overlay") == 0)
        b.overlay = lua_toboolean(L, 3) != 0;
    else if (std::strcmp(field, "accent") == 0)
        b.accent = lua_toboolean(L, 3) != 0;
    return 0;
}

int l_ui_clicked(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = scene->GetEntity(tag);
    if (e.GetHandle() == entt::null || !e.HasComponent<ButtonComponent>())
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    auto& b = e.GetComponent<ButtonComponent>();
    const int hit = b.clicked_up ? 1 : 0;
    b.clicked_up = false; // consume sticky latch
    lua_pushboolean(L, hit);
    return 1;
}

int l_ui_hover(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = scene->GetEntity(tag);
    if (e.GetHandle() == entt::null || !e.HasComponent<ButtonComponent>())
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, e.GetComponent<ButtonComponent>().hover);
    return 1;
}

void RegisterUiBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(settings_get_bloom, l_settings_get_bloom);
    BIND(settings_set_bloom, l_settings_set_bloom);
    BIND(settings_get_vignette, l_settings_get_vignette);
    BIND(settings_set_vignette, l_settings_set_vignette);
    BIND(settings_get_reduce_flash, l_settings_get_reduce_flash);
    BIND(settings_set_reduce_flash, l_settings_set_reduce_flash);
    BIND(settings_get_volume, l_settings_get_volume);
    BIND(settings_set_volume, l_settings_set_volume);
    BIND(set_text, l_set_text);
    BIND(set_font, l_set_font);
    BIND(get_text, l_get_text);
    BIND(emit_burst, l_emit_burst);
    BIND(emit_trail, l_emit_trail);
    BIND(emit_splash, l_emit_splash);
    BIND(emit_mote, l_emit_mote);
    BIND(set_emitting, l_set_emitting);
    BIND(destroy, l_destroy_hint);
    BIND(despawn, l_despawn);
    BIND(restore, l_restore_hint);
    BIND(spawn_box, l_spawn_box);
    BIND(ui_button, l_ui_button);
    BIND(ui_set_default_font, l_ui_set_default_font);
    BIND(ui_set, l_ui_set);
    BIND(ui_clicked, l_ui_clicked);
    BIND(ui_hover, l_ui_hover);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterUiBinds(lua_State* L)
{
    RegisterUiBindsImpl(L);
}

#endif // FW_HAS_LUAU
