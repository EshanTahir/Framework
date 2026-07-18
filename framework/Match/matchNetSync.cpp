#include "matchNetSync.h"
#include "matchSession.h"
#include "matchNetModel.h"
#include "matchChannels.h"
#include "matchEvents.h"
#include "simClock.h"
#include "inputSampler.h"
#include "inputFrame.h"
#include "Match/matchConfig.h"
#include "Scene/components.h"
#include "Net/netSession.h"

#include "box2d.h"

#include <vector>
#include <cstring>
#include <algorithm>

namespace MatchNetSync
{
    Config& GetConfig()
    {
        static Config c;
        return c;
    }

    void AfterSim(entt::registry& registry, bool send_local_input)
    {
        auto& session = MatchSession::GetInstance();
        const Config& cfg = GetConfig();
        if (session.Authority() == MatchAuthority::OfflineHost)
            return;
        // Rollback games exchange inputs + resim locally - no soft pawn snapshots.
        if (session.NetModel() == MatchNetModel::Rollback)
            return;

        if (session.IsNetServer())
        {
            auto& host = NetHost::GetInstance();
            if (!host.IsListening())
                return;

            std::vector<NetPawnState> pawns;
            auto view = registry.view<PlayerInputComponent, TransformComponent, RigidBody2DComponent>();
            for (auto e : view)
            {
                auto& pin = view.get<PlayerInputComponent>(e);
                auto& t = view.get<TransformComponent>(e);
                auto& rb = view.get<RigidBody2DComponent>(e);
                NetPawnState p;
                p.slot = pin.slot;
                p.x = t.x;
                p.y = t.y;
                if (rb.body)
                {
                    b2Vec2 v = rb.body->GetLinearVelocity();
                    p.vx = v.x;
                    p.vy = v.y;
                }
                p.flags = NetPawnFlags::Alive;
                if (session.ConsumeForceSnap(pin.slot))
                    p.flags |= NetPawnFlags::ForceSnap;
                pawns.push_back(p);
            }

            std::string ch;
            MatchChannels::GetInstance().ExportDirty(ch);

            NetSnapshotHeader hdr;
            hdr.tick = SimClock::GetInstance().Tick();
            hdr.slot_count = (uint8_t)std::min<size_t>(pawns.size(), 255);
            hdr.channel_bytes = (uint16_t)std::min<size_t>(ch.size(), 65535);

            std::string payload;
            payload.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
            for (uint8_t i = 0; i < hdr.slot_count; ++i)
                payload.append(reinterpret_cast<const char*>(&pawns[i]), sizeof(NetPawnState));
            if (hdr.channel_bytes)
                payload.append(ch.data(), hdr.channel_bytes);

            host.Broadcast(NetMsg::Snapshot, payload.data(), payload.size());
            MatchChannels::GetInstance().ClearDirty();

            for (const MatchEventWire& ev : MatchEvents::GetInstance().TakeOutbound())
                host.Broadcast(NetMsg::Event, &ev, sizeof(ev));
        }
        else if (session.IsClient())
        {
            auto& client = NetClient::GetInstance();
            if (!client.IsConnected())
                return;

            if (send_local_input)
            {
                const InputFrame& f = InputSampler::GetInstance().Frame(session.LocalControlledSlot());
                client.Send(NetMsg::Input, &f, sizeof(f));
            }

            auto pawns = client.TakePawns();
            std::string ch = client.TakeChannelBlob();
            if (!ch.empty())
                MatchChannels::GetInstance().ImportBlob(ch.data(), ch.size());

            for (const MatchEventWire& ev : client.TakeEvents())
                MatchEvents::GetInstance().PushInbound(ev);

            const uint64_t snap_tick = client.LastSnapshotTick();
            if (snap_tick)
                SimClock::GetInstance().SoftSyncToRemote(snap_tick);

            auto view = registry.view<PlayerInputComponent, TransformComponent, RigidBody2DComponent>();
            for (const auto& p : pawns)
            {
                for (auto e : view)
                {
                    auto& pin = view.get<PlayerInputComponent>(e);
                    if (pin.slot != p.slot)
                        continue;
                    auto& t = view.get<TransformComponent>(e);
                    auto& rb = view.get<RigidBody2DComponent>(e);
                    const bool local = (p.slot == session.LocalControlledSlot());
                    const bool force_snap = (p.flags & NetPawnFlags::ForceSnap) != 0;

                    if (local && !force_snap)
                    {
                        float dx = p.x - t.x;
                        float dy = p.y - t.y;
                        float err2 = dx * dx + dy * dy;
                        float dvy = 0.f;
                        if (rb.body)
                            dvy = std::fabs(p.vy - rb.body->GetLinearVelocity().y);
                        if (err2 > cfg.hard_err * cfg.hard_err || dvy > cfg.hard_dvy)
                        {
                            t.x = p.x;
                            t.y = p.y;
                            if (rb.body)
                            {
                                rb.body->SetTransform(b2Vec2(p.x, p.y), rb.body->GetAngle());
                                rb.body->SetLinearVelocity(b2Vec2(p.vx, p.vy));
                            }
                        }
                        else if (err2 > cfg.soft_err * cfg.soft_err)
                        {
                            t.x += dx * cfg.blend;
                            t.y += dy * cfg.blend;
                            if (rb.body)
                                rb.body->SetTransform(b2Vec2(t.x, t.y), rb.body->GetAngle());
                        }
                    }
                    else
                    {
                        t.x = p.x;
                        t.y = p.y;
                        if (rb.body)
                        {
                            rb.body->SetTransform(b2Vec2(p.x, p.y), rb.body->GetAngle());
                            rb.body->SetLinearVelocity(b2Vec2(p.vx, p.vy));
                        }
                    }
                    break;
                }
            }
        }
    }
}
