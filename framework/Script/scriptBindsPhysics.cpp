#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Scene/sceneHandler.h"
#include "box2d.h"
#include "lua.h"
#include "lualib.h"
#include <cmath>
#include <cstring>

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

int l_get_xy(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<TransformComponent>(e))
    { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    auto& t = g_registry->get<TransformComponent>(e);
    lua_pushnumber(L, t.x);
    lua_pushnumber(L, t.y);
    return 2;
}

int l_set_xy(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<TransformComponent>(e)) return 0;
    auto& t = g_registry->get<TransformComponent>(e);
    t.x = x; t.y = y;
    if (g_registry->has<RigidBody2DComponent>(e))
        SyncBodyWorldPos(g_registry->get<RigidBody2DComponent>(e).body, x, y);
    return 0;
}

int l_get_angle(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry) { lua_pushnumber(L, 0); return 1; }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<TransformComponent>(e)) { lua_pushnumber(L, 0); return 1; }
    lua_pushnumber(L, g_registry->get<TransformComponent>(e).a);
    return 1;
}

int l_set_angle(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float a = (float)luaL_checknumber(L, 2);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<TransformComponent>(e)) return 0;
    g_registry->get<TransformComponent>(e).a = a;
    if (g_registry->has<RigidBody2DComponent>(e))
    {
        auto& rb = g_registry->get<RigidBody2DComponent>(e);
        if (rb.body)
        {
            auto p = rb.body->GetPosition();
            rb.body->SetTransform(p, a);
        }
    }
    return 0;
}

int l_get_scale(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    float s = 1.f;
    if (g_registry && g_registry->valid(e) && g_registry->has<TransformComponent>(e))
        s = g_registry->get<TransformComponent>(e).s;
    lua_pushnumber(L, s);
    return 1;
}

int l_set_scale(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    float s = (float)luaL_checknumber(L, 2);
    if (!g_registry || !g_registry->valid(e) || !g_registry->has<TransformComponent>(e))
        return 0;
    g_registry->get<TransformComponent>(e).s = s;
    return 0;
}

int l_get_velocity(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<RigidBody2DComponent>(e))
    { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    b2Vec2 v = rb.body ? rb.body->GetLinearVelocity() : rb.velocity;
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    return 2;
}

int l_set_velocity(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float vx = (float)luaL_checknumber(L, 2);
    float vy = (float)luaL_checknumber(L, 3);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<RigidBody2DComponent>(e)) return 0;
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    rb.velocity = {vx, vy};
    if (rb.body)
        rb.body->SetLinearVelocity(b2Vec2(vx, vy));
    return 0;
}

int l_apply_impulse(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float ix = (float)luaL_checknumber(L, 2);
    float iy = (float)luaL_checknumber(L, 3);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<RigidBody2DComponent>(e)) return 0;
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    if (rb.body)
        rb.body->ApplyLinearImpulseToCenter(b2Vec2(ix, iy), true);
    return 0;
}

int l_apply_force(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float fx = (float)luaL_checknumber(L, 2);
    float fy = (float)luaL_checknumber(L, 3);
    if (!g_registry) return 0;
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<RigidBody2DComponent>(e)) return 0;
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    if (rb.body)
        rb.body->ApplyForceToCenter(b2Vec2(fx, fy), true);
    return 0;
}

int l_set_gravity_scale(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    if (!g_registry || !g_registry->valid(e) || !g_registry->has<RigidBody2DComponent>(e))
        return 0;
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    rb.gravity_scale = g;
    if (rb.body)
        rb.body->SetGravityScale(g);
    return 0;
}

int l_is_on_ground(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    bool g = false;
    if (g_registry)
    {
        auto e = EntityFromId(id);
        if (g_registry->valid(e) && g_registry->has<CollisionComponent>(e))
            g = g_registry->get<CollisionComponent>(e).on_ground;
    }
    lua_pushboolean(L, g);
    return 1;
}

int l_get_box_size(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    float w = 0, h = 0;
    if (g_registry && g_registry->valid(e) && g_registry->has<BoxShapeComponent>(e))
    {
        auto& b = g_registry->get<BoxShapeComponent>(e);
        w = b.width; h = b.height;
    }
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

int l_set_box_size(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    float w = (float)luaL_checknumber(L, 2);
    float h = (float)luaL_checknumber(L, 3);
    if (!g_registry || !g_registry->valid(e) || !g_registry->has<BoxShapeComponent>(e))
        return 0;
    if (w < 0.05f) w = 0.05f;
    if (h < 0.05f) h = 0.05f;

    auto& boxShape = g_registry->get<BoxShapeComponent>(e);
    const float old_h = boxShape.height;
    boxShape.width = w;
    boxShape.height = h;

    // Keep feet planted when height changes (Y+ down)
    const float dy = (old_h - h) * 0.5f;
    if (std::fabs(dy) > 1e-5f && g_registry->has<TransformComponent>(e))
    {
        auto& t = g_registry->get<TransformComponent>(e);
        t.y += dy;
        if (g_registry->has<RigidBody2DComponent>(e))
        {
            auto& rb = g_registry->get<RigidBody2DComponent>(e);
            if (rb.body)
                rb.body->SetTransform(b2Vec2(t.x, t.y), rb.body->GetAngle());
        }
    }

    if (!g_registry->has<RigidBody2DComponent>(e))
        return 0;
    auto& rb = g_registry->get<RigidBody2DComponent>(e);
    if (!rb.body)
        return 0;

    Collider2DComponent* col = g_registry->has<Collider2DComponent>(e)
        ? &g_registry->get<Collider2DComponent>(e)
        : nullptr;

    // Prefer in-place polygon resize so ground contacts survive (slide squash)
    b2Fixture* main_fx = (col && col->fixture) ? col->fixture : nullptr;
    if (!main_fx)
    {
        for (b2Fixture* f = rb.body->GetFixtureList(); f; f = f->GetNext())
        {
            if (!f->IsSensor() && f->GetType() == b2Shape::e_polygon)
            {
                main_fx = f;
                break;
            }
        }
    }
    if (main_fx && main_fx->GetType() == b2Shape::e_polygon)
    {
        auto* poly = (b2PolygonShape*)main_fx->GetShape();
        poly->SetAsBox(w * 0.5f, h * 0.5f);
        if (boxShape.shape)
            boxShape.shape->SetAsBox(w * 0.5f, h * 0.5f);
        else
        {
            boxShape.shape = new b2PolygonShape();
            boxShape.shape->SetAsBox(w * 0.5f, h * 0.5f);
        }
        // Foot sensor rides under the box - reshape without destroying main fixture
        for (b2Fixture* f = rb.body->GetFixtureList(); f; f = f->GetNext())
        {
            if (f->IsSensor() && f->GetType() == b2Shape::e_polygon)
            {
                auto* foot = (b2PolygonShape*)f->GetShape();
                foot->SetAsBox(w * 0.45f, 0.1f, b2Vec2(0, h * 0.5f), 0);
            }
        }
        rb.body->SetAwake(true);
        return 0;
    }

    // Fallback: full fixture rebuild (no live polygon to mutate)
    const b2Vec2 keep_v = rb.body->GetLinearVelocity();
    const float keep_w = rb.body->GetAngularVelocity();

    for (b2Fixture* f = rb.body->GetFixtureList(); f;)
    {
        b2Fixture* next = f->GetNext();
        rb.body->DestroyFixture(f);
        f = next;
    }
    if (boxShape.shape)
    {
        delete boxShape.shape;
        boxShape.shape = nullptr;
    }

    MaterialComponent mat;
    if (g_registry->has<MaterialComponent>(e))
        mat = g_registry->get<MaterialComponent>(e);
    else
    {
        mat.density = 1.f;
        mat.friction = 0.f;
        mat.restitution = 0.f;
    }

    bool sensor = false;
    uint16_t cat = 0x0001, mask = 0xFFFF;
    bool foot = false;
    if (col)
    {
        sensor = col->sensor;
        cat = col->category_bits;
        mask = col->mask_bits;
        foot = col->is_ground_sensor;
        col->fixture = nullptr;
    }

    b2FixtureDef fd;
    fd.density = mat.density;
    fd.friction = mat.friction;
    fd.restitution = mat.restitution;
    fd.isSensor = sensor;
    fd.filter.categoryBits = cat;
    fd.filter.maskBits = mask;
    boxShape.shape = new b2PolygonShape();
    boxShape.shape->SetAsBox(w * 0.5f, h * 0.5f);
    fd.shape = boxShape.shape;
    b2Fixture* created = rb.body->CreateFixture(&fd);
    if (col)
        col->fixture = created;

    if (foot)
    {
        b2PolygonShape sensor_shape;
        sensor_shape.SetAsBox(w * 0.45f, 0.1f, b2Vec2(0, h * 0.5f), 0);
        b2FixtureDef sfd;
        sfd.shape = &sensor_shape;
        sfd.isSensor = true;
        sfd.density = 0;
        sfd.filter.categoryBits = 0x0002;
        sfd.filter.maskBits = 0x0001;
        rb.body->CreateFixture(&sfd);
    }

    rb.body->SetLinearVelocity(keep_v);
    rb.body->SetAngularVelocity(keep_w);
    return 0;
}

int l_raycast(lua_State* L)
{
    float x1 = (float)luaL_checknumber(L, 1);
    float y1 = (float)luaL_checknumber(L, 2);
    float x2 = (float)luaL_checknumber(L, 3);
    float y2 = (float)luaL_checknumber(L, 4);
    bool ignore_sensors = true;
    if (lua_gettop(L) >= 5)
        ignore_sensors = lua_toboolean(L, 5) != 0;

    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !scene->world)
    {
        lua_pushboolean(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnil(L);
        return 6;
    }

    struct Hit
    {
        bool ok = false;
        b2Vec2 point{0, 0};
        b2Vec2 normal{0, 0};
        float fraction = 1.f;
        uint32_t entity = 0;
    } hit;

    class CB : public b2RayCastCallback
    {
    public:
        bool ignore_sensors = true;
        Hit* out = nullptr;
        float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                           const b2Vec2& normal, float fraction) override
        {
            if (ignore_sensors && fixture->IsSensor())
                return -1.f; // continue
            out->ok = true;
            out->point = point;
            out->normal = normal;
            out->fraction = fraction;
            out->entity = static_cast<uint32_t>(
                static_cast<uint32_t>(fixture->GetBody()->GetUserData().pointer));
            return fraction; // clip to closest
        }
    } cb;
    cb.ignore_sensors = ignore_sensors;
    cb.out = &hit;

    // Box2D asserts on zero-length rays (b2_dynamic_tree LengthSquared)
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    if (dx * dx + dy * dy < 1e-12f)
    {
        lua_pushboolean(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnil(L);
        return 6;
    }

    scene->world->RayCast(&cb, b2Vec2(x1, y1), b2Vec2(x2, y2));

    lua_pushboolean(L, hit.ok ? 1 : 0);
    lua_pushnumber(L, hit.point.x);
    lua_pushnumber(L, hit.point.y);
    lua_pushnumber(L, hit.normal.x);
    lua_pushnumber(L, hit.normal.y);
    if (hit.ok)
        lua_pushinteger(L, (int)hit.entity);
    else
        lua_pushnil(L);
    return 6;
}

int l_rope_attach(lua_State* L)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !scene->world)
    {
        lua_pushboolean(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }
    const entt::entity owner_e = ResolveArg(L, 1);
    b2Body* a = BodyOf(owner_e);
    b2Body* b = BodyOf(ResolveArg(L, 2));
    if (!a || !b || a == b)
    {
        lua_pushboolean(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }
    const uint32_t owner = (uint32_t)owner_e;

    DropRopeFor(owner);

    const b2Vec2 pa = a->GetPosition();
    const b2Vec2 pb = b->GetPosition();
    float len = (float)luaL_optnumber(L, 3, (pb - pa).Length());
    if (len < 0.1f) len = 0.1f;
    float min_len = (float)luaL_optnumber(L, 4, 0.0);
    if (min_len < 0.f) min_len = 0.f;
    if (min_len > len) min_len = len;

    b2DistanceJointDef def;
    def.Initialize(a, b, pa, pb);
    def.collideConnected = true;
    def.length = len;
    def.minLength = min_len;
    def.maxLength = len;
    def.stiffness = 0.f; // no spring: free inside the range, rigid at the limits
    def.damping = 0.f;

    RopeHandle& h = g_ropes[owner];
    h.joint = (b2DistanceJoint*)scene->world->CreateJoint(&def);
    h.world = scene->world;
    lua_pushboolean(L, h.joint != nullptr);
    lua_pushnumber(L, len);
    return 2;
}

int l_rope_release(lua_State* L)
{
    DropRopeFor((uint32_t)ResolveArg(L, 1));
    return 0;
}

int l_rope_set_length(lua_State* L)
{
    const uint32_t owner = (uint32_t)ResolveArg(L, 1);
    float len = (float)luaL_checknumber(L, 2);
    if (len < 0.1f) len = 0.1f;
    b2DistanceJoint* rope = LiveRope(owner);
    if (!rope)
    {
        lua_pushnumber(L, 0);
        return 1;
    }
    rope->SetLength(len);
    rope->SetMaxLength(len);
    lua_pushnumber(L, rope->GetLength());
    return 1;
}

int l_rope_active(lua_State* L)
{
    lua_pushboolean(L, LiveRope((uint32_t)ResolveArg(L, 1)) != nullptr);
    return 1;
}

int l_touching_count(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int n = 0;
    if (g_registry)
    {
        auto e = EntityFromId(id);
        if (g_registry->valid(e) && g_registry->has<CollisionComponent>(e))
            n = (int)g_registry->get<CollisionComponent>(e).touching.size();
    }
    lua_pushinteger(L, n);
    return 1;
}

int l_get_touching(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2); // 1-based
    if (!g_registry) { lua_pushnil(L); return 1; }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<CollisionComponent>(e))
    { lua_pushnil(L); return 1; }
    auto& c = g_registry->get<CollisionComponent>(e);
    if (index < 1 || index > (int)c.touching.size())
    { lua_pushnil(L); return 1; }
    lua_pushinteger(L, (int)c.touching[index - 1]);
    return 1;
}

int l_contact_event_count(lua_State* L)
{
    (void)L;
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, (int)scene->Contacts().events.size());
    return 1;
}

int l_get_contact_event(lua_State* L)
{
    // 1-based → a, b, begin, sensor (or nil)
    int index = (int)luaL_checkinteger(L, 1);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene)
    {
        lua_pushnil(L);
        return 1;
    }
    const auto& evs = scene->Contacts().events;
    if (index < 1 || index > (int)evs.size())
    {
        lua_pushnil(L);
        return 1;
    }
    const auto& ev = evs[(size_t)index - 1];
    lua_pushinteger(L, (lua_Integer)ev.a);
    lua_pushinteger(L, (lua_Integer)ev.b);
    lua_pushboolean(L, ev.begin ? 1 : 0);
    lua_pushboolean(L, ev.sensor ? 1 : 0);
    return 4;
}

void RegisterPhysicsBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(get_xy, l_get_xy);
    BIND(set_xy, l_set_xy);
    BIND(get_angle, l_get_angle);
    BIND(set_angle, l_set_angle);
    BIND(get_scale, l_get_scale);
    BIND(set_scale, l_set_scale);
    BIND(get_velocity, l_get_velocity);
    BIND(set_velocity, l_set_velocity);
    BIND(apply_impulse, l_apply_impulse);
    BIND(apply_force, l_apply_force);
    BIND(set_gravity_scale, l_set_gravity_scale);
    BIND(is_on_ground, l_is_on_ground);
    BIND(get_box_size, l_get_box_size);
    BIND(set_box_size, l_set_box_size);
    BIND(raycast, l_raycast);
    BIND(rope_attach, l_rope_attach);
    BIND(rope_release, l_rope_release);
    BIND(rope_set_length, l_rope_set_length);
    BIND(rope_active, l_rope_active);
    BIND(touching_count, l_touching_count);
    BIND(get_touching, l_get_touching);
    BIND(contact_event_count, l_contact_event_count);
    BIND(get_contact_event, l_get_contact_event);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterPhysicsBinds(lua_State* L)
{
    RegisterPhysicsBindsImpl(L);
}

#endif // FW_HAS_LUAU
