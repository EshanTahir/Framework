#include "entity.h"
#include "scene.h"
#include "Script/scriptEngine.h"
#include "Match/simClock.h"
#include "Match/matchSession.h"
#include "Match/matchNetModel.h"
#include "Match/matchNetSync.h"
#include "Match/matchEvents.h"
#include "Match/inputSampler.h"
#include "Match/rollbackSession.h"
#include "Match/matchConfig.h"
#include "Net/netSession.h"
#include "Net/lobbyClient.h"
#include "Render/gl.h"
#include "Core/log.h"

#include <algorithm>
#include <iostream>
#include <vector>

Scene::Scene()
{
    b2Vec2 gravity(0.0f, 9.81f);
    world = new b2World(gravity);
}

Scene::~Scene()
{
    delete world;
}

Entity Scene::CreateEntity(const std::string& tag)
{
    Entity entity(registry.create(), this);
    entity.AddComponent<TagComponent>(tag);
    tag_entity[tag] = entity.GetHandle();
    return entity;
}

Entity Scene::GetEntity(const std::string& tag)
{
    auto it = tag_entity.find(tag);
    if (it == tag_entity.end())
        return Entity(entt::null, this);
    if (!registry.valid(it->second))
    {
        tag_entity.erase(it);
        return Entity(entt::null, this);
    }
    return Entity(it->second, this);
}

std::vector<std::string> Scene::ListTags() const
{
    std::vector<std::string> out;
    out.reserve(tag_entity.size());
    for (const auto& kv : tag_entity)
    {
        if (registry.valid(kv.second))
            out.push_back(kv.first);
    }
    return out;
}

void Scene::DestroyEntity(entt::entity handle)
{
    if (handle == entt::null || !registry.valid(handle))
        return;

    if (registry.has<ScriptComponent>(handle))
        ScriptEngine::GetInstance().DropEntity((uint32_t)handle);

    if (registry.has<TextComponent>(handle))
    {
        auto& text = registry.get<TextComponent>(handle);
        if (text.cached_tex)
        {
            GLuint id = text.cached_tex;
            glDeleteTextures(1, &id);
            text.cached_tex = 0;
        }
    }

    if (registry.has<ButtonComponent>(handle))
    {
        auto& b = registry.get<ButtonComponent>(handle);
        if (b.cached_tex)
        {
            GLuint id = b.cached_tex;
            glDeleteTextures(1, &id);
            b.cached_tex = 0;
        }
    }

    if (registry.has<RigidBody2DComponent>(handle))
    {
        auto& rb = registry.get<RigidBody2DComponent>(handle);
        if (rb.body && world)
        {
            world->DestroyBody(rb.body);
            rb.body = nullptr;
            rb.initialized = false;
        }
    }

    // Box2D copies shape data into fixtures; our heap shapes must still be freed.
    if (registry.has<BoxShapeComponent>(handle))
    {
        delete registry.get<BoxShapeComponent>(handle).shape;
        registry.get<BoxShapeComponent>(handle).shape = nullptr;
    }
    if (registry.has<CircleShapeComponent>(handle))
    {
        delete registry.get<CircleShapeComponent>(handle).shape;
        registry.get<CircleShapeComponent>(handle).shape = nullptr;
    }
    if (registry.has<PolygonShapeComponent>(handle))
    {
        delete registry.get<PolygonShapeComponent>(handle).shape;
        registry.get<PolygonShapeComponent>(handle).shape = nullptr;
    }
    if (registry.has<EdgeShapeComponent>(handle))
    {
        delete registry.get<EdgeShapeComponent>(handle).shape;
        registry.get<EdgeShapeComponent>(handle).shape = nullptr;
    }
    if (registry.has<ChainShapeComponent>(handle))
    {
        delete registry.get<ChainShapeComponent>(handle).shape;
        registry.get<ChainShapeComponent>(handle).shape = nullptr;
    }

    if (registry.has<TagComponent>(handle))
    {
        const std::string& tag = registry.get<TagComponent>(handle).tag;
        auto it = tag_entity.find(tag);
        if (it != tag_entity.end() && it->second == handle)
            tag_entity.erase(it);
    }

    registry.destroy(handle);
}

void Scene::DestroyEntity(const std::string& tag)
{
    auto it = tag_entity.find(tag);
    if (it == tag_entity.end())
        return;
    DestroyEntity(it->second);
}

void Scene::ClearAllEntities()
{
    std::vector<entt::entity> handles;
    handles.reserve(tag_entity.size());
    for (auto& kv : tag_entity)
    {
        if (registry.valid(kv.second))
            handles.push_back(kv.second);
    }
    // Catch anything that lost its tag map entry but is still alive.
    registry.each([&](entt::entity e) {
        if (registry.valid(e))
            handles.push_back(e);
    });
    std::sort(handles.begin(), handles.end());
    handles.erase(std::unique(handles.begin(), handles.end()), handles.end());
    for (entt::entity e : handles)
        DestroyEntity(e);
    tag_entity.clear();
}

void Scene::FixedUpdate(float sim_dt, bool resample_input)
{
    // Capture pre-step poses for render lerp (Fix Your Timestep style).
    {
        auto view = registry.view<TransformComponent>();
        for (auto e : view)
        {
            auto& t = registry.get<TransformComponent>(e);
            t.ix = t.x;
            t.iy = t.y;
            t.ia = t.a;
            t.interp_primed = true;
        }
    }

    auto& session = MatchSession::GetInstance();
    auto& clock = SimClock::GetInstance();
    auto& rb = MatchRollback::GetInstance();
    auto& mc = MatchConfig::GetInstance();

    NetHost::GetInstance().Poll();
    NetClient::GetInstance().Poll();
    LobbyClient::GetInstance().Poll();

    // Rollback net: feed peer inputs into history (may resim) before stepping.
    if (session.NetModel() == MatchNetModel::Rollback && rb.Active())
    {
        rb.SetSlotCount(std::max(1, mc.Count()));
        rb.SetLocalSlot(session.LocalControlledSlot());
        {
            uint32_t mask = 0;
            const int local = session.LocalControlledSlot();
            for (int i = 0; i < mc.Count(); ++i)
            {
                const MatchSlot* s = mc.Slot(i);
                if (!s || s->vacant)
                    continue;
                if (i != local)
                    mask |= (1u << i);
            }
            rb.SetRemoteSlotMask(mask);
        }
        rb.on_resim_exhausted = [](uint64_t tick) {
            if (MatchSession::GetInstance().IsClient() && NetClient::GetInstance().IsConnected())
                NetClient::GetInstance().Send(NetMsg::StateBlobReq, &tick, sizeof(tick));
        };

        if (session.IsHost())
        {
            auto inputs = NetHost::GetInstance().TakeInputs();
            if (!inputs.empty())
            {
                rb.OnRemoteInputs(inputs.data(), inputs.size());
                // Fan-out peer inputs to other clients (3+ player rollback).
                for (const InputFrame& f : inputs)
                    NetHost::GetInstance().Broadcast(NetMsg::Input, &f, sizeof(f));
            }
        }
        else if (session.IsClient())
        {
            auto peer = NetClient::GetInstance().TakePeerInputs();
            if (!peer.empty())
                rb.OnRemoteInputs(peer.data(), peer.size());

            for (const auto& cs : NetClient::GetInstance().TakeChecksums())
            {
                const uint64_t local = rb.HashSavedTick(cs.tick);
                if (local && local != cs.hash)
                {
                    FW_CORE_WARN("Rollback checksum mismatch tick {0}", cs.tick);
                    NetClient::GetInstance().Send(NetMsg::StateBlobReq, &cs.tick, sizeof(cs.tick));
                }
            }
            for (auto& blob : NetClient::GetInstance().TakeStateBlobs())
                rb.ApplyPanicBlob(blob.first, reinterpret_cast<const uint8_t*>(blob.second.data()),
                                  blob.second.size());

            for (const MatchEventWire& ev : NetClient::GetInstance().TakeEvents())
                MatchEvents::GetInstance().PushInbound(ev, 0);
        }
        MatchEvents::GetInstance().ReleaseConfirmed(rb.ConfirmedTick());
    }

    session.BeginSimTick(clock.Tick(), resample_input);
    if (session.NetModel() != MatchNetModel::Rollback || !rb.Active())
    {
        if (session.IsHost())
        {
            auto inputs = NetHost::GetInstance().TakeInputs();
            if (!inputs.empty())
                session.ApplyRemoteInputQueue(inputs.data(), inputs.size());
        }
    }
    session.RecordSimInputs();

    if (session.NetModel() == MatchNetModel::Rollback && rb.Active())
    {
        rb.SetLocalSlot(session.LocalControlledSlot());
        rb.on_send_local = [&](const InputFrame& f) {
            if (session.IsClient() && NetClient::GetInstance().IsConnected())
                NetClient::GetInstance().Send(NetMsg::Input, &f, sizeof(f));
            else if (session.IsNetServer() && NetHost::GetInstance().IsListening())
                NetHost::GetInstance().Broadcast(NetMsg::Input, &f, sizeof(f));
        };
        rb.RunTick(clock.Tick(), sim_dt);
        MatchEvents::GetInstance().ReleaseConfirmed(rb.ConfirmedTick());

        if (session.IsNetServer() && NetHost::GetInstance().IsListening())
        {
            for (const MatchEventWire& ev : MatchEvents::GetInstance().TakeOutbound())
                NetHost::GetInstance().Broadcast(NetMsg::Event, &ev, sizeof(ev));

            const uint64_t tick = clock.Tick();
            if (tick > 0 && (tick % 30ull) == 0)
            {
                NetStateChecksum cs{};
                cs.tick = tick;
                cs.hash = rb.HashSavedTick(tick);
                if (cs.hash)
                    NetHost::GetInstance().Broadcast(NetMsg::StateChecksum, &cs, sizeof(cs));
            }
            for (uint64_t req_tick : NetHost::GetInstance().TakeStateBlobReqs())
            {
                std::vector<uint8_t> blob;
                if (!rb.GetSavedBlob(req_tick, blob))
                    continue;
                NetStateBlobHeader hdr{};
                hdr.tick = req_tick;
                hdr.byte_count = (uint32_t)blob.size();
                std::string payload;
                payload.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
                payload.append(reinterpret_cast<const char*>(blob.data()), blob.size());
                NetHost::GetInstance().Broadcast(NetMsg::StateBlob, payload.data(), payload.size());
            }
        }
    }
    else
    {
        scripts.Run(registry, sim_dt);
        physics.Run(registry, *world, sim_dt);
        scripts.RunPostPhys(registry, sim_dt);
        MatchNetSync::AfterSim(registry, resample_input);
    }

    clock.AdvanceTick();
}

void Scene::FrameUpdate(float real_dt)
{
    particles.Run(registry, real_dt);
    renderer.Run(registry, real_dt);
    // Dim overlays under pause/menu labels; chrome buttons on top.
    ui.DrawOverlays(registry);
    renderer.DrawScreenHud(registry);
    ui.DrawChrome(registry);
    sound.Run(registry, real_dt);
    debug_hud.Run(registry, real_dt);
}

void Scene::Update(float real_dt)
{
    if (renderer.window == nullptr)
        renderer.window = Window::GetInstance().GetWindow();

    // Once per display frame: hot-reload / edge bookkeeping for menu paths.
    ScriptEngine::GetInstance().BeginFrame(real_dt);

    // Latch press edges even when this display frame runs 0 sim steps (common on
    // high refresh rates): otherwise Btn*Pressed dies before a sim tick consumes it.
    InputSampler::GetInstance().LatchDisplayEdges();

    // UI click latch before FixedUpdate scripts - same-frame ui_clicked, and sticky
    // across 0-step display frames (see ButtonComponent::clicked_up).
    ui.ProcessInput(registry);

    // Apply async net/lobby worker results even when this display frame runs 0 sim steps.
    NetHost::GetInstance().Poll();
    NetClient::GetInstance().Poll();
    LobbyClient::GetInstance().Poll();

    auto& clock = SimClock::GetInstance();
    const int steps = clock.BeginFrame(real_dt);
    const float sim_dt = clock.SimDt();
    if (clock.Paused())
    {
        // Sim frozen - still tick pause overlay so Esc / buttons work.
        scripts.RunTagged(registry, "pause_ui", real_dt);
    }
    else
    {
        for (int i = 0; i < steps; ++i)
            FixedUpdate(sim_dt, i == 0);
    }

    FrameUpdate(real_dt);
}

void Scene::Initialize()
{
    auto view = registry.view<ScriptComponent>();
    for (auto e : view)
    {
        auto& sc = registry.get<ScriptComponent>(e);
        sc.entering_scene = true;
        sc.booted = false;
        ScriptEngine::GetInstance().DropEntity((uint32_t)e);
    }
    physics.Initialize(registry, *world);
}

void Scene::UnInitialize()
{
    physics.UnInitialize(registry, *world);
}
