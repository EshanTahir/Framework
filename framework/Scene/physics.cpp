#include "systems.h"
#include "Core/assetCache.h"
#include "Core/log.h"
#include "Core/paths.h"

#include <cmath>
#include <json.hpp>

namespace
{
    void FreeOwnedShapes(entt::registry& registry, entt::entity entity)
    {
        if (!registry.valid(entity))
            return;
        if (registry.has<BoxShapeComponent>(entity))
        {
            auto& s = registry.get<BoxShapeComponent>(entity);
            delete s.shape;
            s.shape = nullptr;
        }
        if (registry.has<CircleShapeComponent>(entity))
        {
            auto& s = registry.get<CircleShapeComponent>(entity);
            delete s.shape;
            s.shape = nullptr;
        }
        if (registry.has<PolygonShapeComponent>(entity))
        {
            auto& s = registry.get<PolygonShapeComponent>(entity);
            delete s.shape;
            s.shape = nullptr;
        }
        if (registry.has<EdgeShapeComponent>(entity))
        {
            auto& s = registry.get<EdgeShapeComponent>(entity);
            delete s.shape;
            s.shape = nullptr;
        }
        if (registry.has<ChainShapeComponent>(entity))
        {
            auto& s = registry.get<ChainShapeComponent>(entity);
            delete s.shape;
            s.shape = nullptr;
        }
    }

    b2Fixture* CreateFixtureForShape(entt::registry& registry, entt::entity entity,
                                     b2Body* body, Shape shape, const MaterialComponent& mat,
                                     bool sensor, uint16_t cat, uint16_t mask)
    {
        b2FixtureDef fd;
        fd.density = mat.density;
        fd.friction = mat.friction;
        fd.restitution = mat.restitution;
        fd.isSensor = sensor;
        fd.filter.categoryBits = cat;
        fd.filter.maskBits = mask;

        if (shape == box && registry.has<BoxShapeComponent>(entity))
        {
            auto& s = registry.get<BoxShapeComponent>(entity);
            delete s.shape;
            s.shape = new b2PolygonShape();
            s.shape->SetAsBox(s.width / 2.f, s.height / 2.f);
            fd.shape = s.shape;
            return body->CreateFixture(&fd);
        }
        if (shape == circle && registry.has<CircleShapeComponent>(entity))
        {
            auto& s = registry.get<CircleShapeComponent>(entity);
            delete s.shape;
            s.shape = new b2CircleShape();
            s.shape->m_radius = s.radius;
            fd.shape = s.shape;
            return body->CreateFixture(&fd);
        }
        if (shape == polygon && registry.has<PolygonShapeComponent>(entity))
        {
            auto& s = registry.get<PolygonShapeComponent>(entity);
            if (s.vertices.size() >= 3)
            {
                delete s.shape;
                s.shape = new b2PolygonShape();
                s.shape->Set(s.vertices.data(), (int32)s.vertices.size());
                fd.shape = s.shape;
                return body->CreateFixture(&fd);
            }
        }
        if (shape == edge && registry.has<EdgeShapeComponent>(entity))
        {
            auto& s = registry.get<EdgeShapeComponent>(entity);
            delete s.shape;
            s.shape = new b2EdgeShape();
            s.shape->SetTwoSided(s.vertex1, s.vertex2);
            fd.shape = s.shape;
            return body->CreateFixture(&fd);
        }
        if (shape == chain && registry.has<ChainShapeComponent>(entity))
        {
            auto& s = registry.get<ChainShapeComponent>(entity);
            if (s.vertices.size() >= 2)
            {
                delete s.shape;
                s.shape = new b2ChainShape();
                if (s.loop)
                    s.shape->CreateLoop(s.vertices.data(), (int32)s.vertices.size());
                else
                    s.shape->CreateChain(s.vertices.data(), (int32)s.vertices.size(),
                                         s.vertices.front(), s.vertices.back());
                fd.shape = s.shape;
                return body->CreateFixture(&fd);
            }
        }
        return nullptr;
    }
}

class Physics::Listener : public b2ContactListener
{
public:
    ContactBuffer* buffer = nullptr;
    entt::registry* registry = nullptr;

    void BeginContact(b2Contact* contact) override { Push(contact, true); }
    void EndContact(b2Contact* contact) override { Push(contact, false); }

    void PreSolve(b2Contact* contact, const b2Manifold*) override
    {
        if (!registry)
            return;
        b2Fixture* fa = contact->GetFixtureA();
        b2Fixture* fb = contact->GetFixtureB();
        auto ea = static_cast<entt::entity>(static_cast<uint32_t>(fa->GetBody()->GetUserData().pointer));
        auto eb = static_cast<entt::entity>(static_cast<uint32_t>(fb->GetBody()->GetUserData().pointer));
        if (!registry->valid(ea) || !registry->valid(eb))
            return;

        auto one_way_vs_dynamic = [&](entt::entity plat, b2Fixture* plat_fx, entt::entity other, b2Body* other_body) -> bool {
            if (!registry->has<Collider2DComponent>(plat) || !registry->get<Collider2DComponent>(plat).one_way)
                return false;
            if (plat_fx->IsSensor() || other_body->GetType() != b2_dynamicBody)
                return false;
            if (!registry->has<TransformComponent>(plat) || !registry->has<BoxShapeComponent>(plat) ||
                !registry->has<TransformComponent>(other) || !registry->has<BoxShapeComponent>(other))
                return false;
            auto& pt = registry->get<TransformComponent>(plat);
            auto& pb = registry->get<BoxShapeComponent>(plat);
            auto& ot = registry->get<TransformComponent>(other);
            auto& ob = registry->get<BoxShapeComponent>(other);
            const float plat_top = pt.y - pb.height * 0.5f;
            const float feet = ot.y + ob.height * 0.5f;
            const float head = ot.y - ob.height * 0.5f;
            const float vy = other_body->GetLinearVelocity().y;
            // Only while under the platform footprint (not a side bump)
            const float dx = (ot.x > pt.x) ? (ot.x - pt.x) : (pt.x - ot.x);
            if (dx > pb.width * 0.5f)
                return false;
            // Y+ down: fully under, or rising through from below
            if (head > plat_top + 0.04f)
                return true;
            if (vy < -0.08f && feet > plat_top + 0.08f)
                return true;
            return false;
        };

        if (one_way_vs_dynamic(ea, fa, eb, fb->GetBody()) || one_way_vs_dynamic(eb, fb, ea, fa->GetBody()))
            contact->SetEnabled(false);
    }

    void Push(b2Contact* contact, bool begin)
    {
        if (!buffer)
            return;
        auto* fa = contact->GetFixtureA();
        auto* fb = contact->GetFixtureB();
        auto* ba = fa->GetBody();
        auto* bb = fb->GetBody();
        auto ea = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ba->GetUserData().pointer));
        auto eb = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bb->GetUserData().pointer));
        buffer->events.push_back({ea, eb, begin, fa->IsSensor() || fb->IsSensor()});
    }
};

void Physics::Initialize(entt::registry& registry, b2World& world)
{
    if (!listener)
    {
        listener = new Listener();
        listener->buffer = &contacts;
        listener->registry = &registry;
        world.SetContactListener(listener);
    }

    BuildTileCollision(registry, world);

    auto view = registry.view<RigidBody2DComponent, TransformComponent>();
    for (auto entity : view)
        InitializeBody(registry, world, entity);
}

void Physics::InitializeBody(entt::registry& registry, b2World& world, entt::entity entity)
{
    if (!registry.valid(entity) || !registry.has<RigidBody2DComponent, TransformComponent>(entity))
        return;

    auto& rb = registry.get<RigidBody2DComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);
    if (rb.initialized)
        return;

    MaterialComponent mat;
    if (registry.has<MaterialComponent>(entity))
        mat = registry.get<MaterialComponent>(entity);
    else
    {
        mat.density = rb.density;
        mat.friction = rb.friction;
        mat.restitution = rb.restitution;
    }

    b2BodyDef def;
    def.type = rb.type;
    def.position.Set(transform.x, transform.y);
    def.angle = transform.a;
    def.gravityScale = rb.gravity_scale;
    def.fixedRotation = rb.fixed_rotation;
    def.bullet = rb.bullet;
    def.linearVelocity = rb.velocity;
    def.angularVelocity = rb.angular_velocity;
    def.userData.pointer = static_cast<uintptr_t>(static_cast<uint32_t>(entity));
    def.enabled = !IsParkedWorldPos(transform.x, transform.y);

    rb.body = world.CreateBody(&def);

    Shape shape = rb.shape;
    bool sensor = false;
    uint16_t cat = 0x0001, mask = 0xFFFF;
    if (registry.has<Collider2DComponent>(entity))
    {
        auto& col = registry.get<Collider2DComponent>(entity);
        shape = col.shape;
        sensor = col.sensor;
        cat = col.category_bits;
        mask = col.mask_bits;
        col.fixture = CreateFixtureForShape(registry, entity, rb.body, shape, mat, sensor, cat, mask);
    }
    else
    {
        CreateFixtureForShape(registry, entity, rb.body, shape, mat, sensor, cat, mask);
    }

    // Optional ground sensor child fixture
    if (registry.has<Collider2DComponent>(entity) &&
        registry.get<Collider2DComponent>(entity).is_ground_sensor &&
        registry.has<BoxShapeComponent>(entity))
    {
        auto& box = registry.get<BoxShapeComponent>(entity);
        b2PolygonShape sensor_shape;
        sensor_shape.SetAsBox(box.width * 0.45f, 0.1f, b2Vec2(0, box.height * 0.5f), 0);
        b2FixtureDef sfd;
        sfd.shape = &sensor_shape;
        sfd.isSensor = true;
        sfd.density = 0;
        sfd.filter.categoryBits = 0x0002;
        sfd.filter.maskBits = 0x0001;
        rb.body->CreateFixture(&sfd);
    }

    if (!registry.has<CollisionComponent>(entity))
        registry.emplace<CollisionComponent>(entity);

    rb.initialized = true;
}

void Physics::BuildTileCollision(entt::registry& registry, b2World& world)
{
    auto view = registry.view<TileMapComponent, TransformComponent, CollideMapComponent>();
    for (auto entity : view)
    {
        auto& tile = registry.get<TileMapComponent>(entity);
        auto& colmap = registry.get<CollideMapComponent>(entity);
        auto& transform = registry.get<TransformComponent>(entity);
        if (colmap.initialized || !colmap.collide_map_location)
            continue;

        auto spawn_tile = [&](float x, float y) {
            b2BodyDef bd;
            bd.type = b2_staticBody;
            bd.position.Set(x, y);
            b2Body* body = world.CreateBody(&bd);
            b2PolygonShape shape;
            shape.SetAsBox(0.5f, 0.5f);
            body->CreateFixture(&shape, 0.f);
            colmap.bodies.push_back(body);
        };

        const auto& json = AssetCache::GetInstance().GetJson(colmap.collide_map_location);
        if (!json.contains("solid") || !json["solid"].is_array())
        {
            if (tile.tile_map_location)
            {
                const auto& grid = AssetCache::GetInstance().GetJson(tile.tile_map_location);
                if (grid.is_array())
                {
                    int y = 0;
                    for (auto& row : grid)
                    {
                        int x = 0;
                        for (auto& cell : row)
                        {
                            int id = cell.get<int>();
                            if (id >= 0)
                                spawn_tile(transform.x + x + 0.5f, transform.y + y + 0.5f);
                            x++;
                        }
                        y++;
                    }
                }
            }
        }
        else
        {
            int y = 0;
            for (auto& row : json["solid"])
            {
                int x = 0;
                for (auto& cell : row)
                {
                    if (cell.get<int>() != 0)
                        spawn_tile(transform.x + x + 0.5f, transform.y + y + 0.5f);
                    x++;
                }
                y++;
            }
        }

        colmap.initialized = true;
        tile.collision_built = true;
    }
}

void Physics::UnInitialize(entt::registry& registry, b2World& world)
{
    auto tile_view = registry.view<CollideMapComponent>();
    for (auto entity : tile_view)
    {
        auto& colmap = registry.get<CollideMapComponent>(entity);
        for (b2Body* b : colmap.bodies)
        {
            if (b)
                world.DestroyBody(b);
        }
        colmap.bodies.clear();
        colmap.initialized = false;
        if (registry.has<TileMapComponent>(entity))
            registry.get<TileMapComponent>(entity).collision_built = false;
    }

    auto view = registry.view<RigidBody2DComponent>();
    for (auto entity : view)
    {
        auto& rb = registry.get<RigidBody2DComponent>(entity);
        if (rb.body)
        {
            world.DestroyBody(rb.body);
            rb.body = nullptr;
        }
        rb.initialized = false;
        FreeOwnedShapes(registry, entity);
        if (registry.has<Collider2DComponent>(entity))
            registry.get<Collider2DComponent>(entity).fixture = nullptr;
    }
}

void Physics::Run(entt::registry& registry, b2World& world, float delta_time)
{
    // Late-created rigidbodies (e.g. engine.spawn_box from Luau, players joining)
    {
        auto view = registry.view<RigidBody2DComponent, TransformComponent>();
        for (auto entity : view)
            InitializeBody(registry, world, entity);
    }

    contacts.Clear();

    // Reset per-frame collision buffers (on_ground rebuilt after Step)
    auto col_view = registry.view<CollisionComponent>();
    for (auto e : col_view)
    {
        auto& c = registry.get<CollisionComponent>(e);
        c.on_ground = false;
        c.touching.clear();
    }

    world.Step(delta_time, velocity_iterations, position_iterations);

    // Sustained contacts: rebuild touching + grounded from the live contact graph
    for (b2Contact* contact = world.GetContactList(); contact; contact = contact->GetNext())
    {
        if (!contact->IsTouching())
            continue;

        b2Fixture* fa = contact->GetFixtureA();
        b2Fixture* fb = contact->GetFixtureB();
        auto ea = static_cast<entt::entity>(static_cast<uint32_t>(fa->GetBody()->GetUserData().pointer));
        auto eb = static_cast<entt::entity>(static_cast<uint32_t>(fb->GetBody()->GetUserData().pointer));

        auto push_touch = [&](entt::entity e, uint32_t other) {
            if (!registry.valid(e) || !registry.has<CollisionComponent>(e))
                return;
            registry.get<CollisionComponent>(e).touching.push_back(other);
        };
        push_touch(ea, static_cast<uint32_t>(eb));
        push_touch(eb, static_cast<uint32_t>(ea));

        // Ground only from solid surfaces under the body (not walls, not sensors)
        b2WorldManifold wm;
        contact->GetWorldManifold(&wm);
        auto try_ground = [&](entt::entity e, b2Fixture* self_fix, b2Fixture* other_fix, float ny_for_self) {
            if (!registry.valid(e) || !registry.has<CollisionComponent>(e))
                return;
            if (other_fix->IsSensor())
                return; // coins / triggers never count as floor
			// Foot sensor vs solid: |ny| so door_grav-flipped players can stand on ceilings.
			if (self_fix->IsSensor())
			{
				if (std::fabs(ny_for_self) > 0.4f)
					registry.get<CollisionComponent>(e).on_ground = true;
				return;
			}
			// Solid body vs solid: floor if normal is mostly vertical relative to this body
			if (!self_fix->IsSensor() && std::fabs(ny_for_self) > 0.5f)
				registry.get<CollisionComponent>(e).on_ground = true;
        };
        // World normal points from A to B; for A, "up" relative to standing is -normal.y
        try_ground(ea, fa, fb, -wm.normal.y);
        try_ground(eb, fb, fa, wm.normal.y);
    }

    auto view = registry.view<RigidBody2DComponent, TransformComponent>();
    for (auto entity : view)
    {
        auto& rb = registry.get<RigidBody2DComponent>(entity);
        auto& transform = registry.get<TransformComponent>(entity);
        if (!rb.body)
            continue;
        b2Vec2 p = rb.body->GetPosition();
        transform.x = p.x;
        transform.y = p.y;
        transform.a = rb.body->GetAngle();
        rb.velocity = rb.body->GetLinearVelocity();
        rb.angular_velocity = rb.body->GetAngularVelocity();
    }
}
