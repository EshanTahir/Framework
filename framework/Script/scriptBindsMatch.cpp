#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Events/inputEvent.h"
#include "Match/inputFrame.h"
#include "Match/inputSampler.h"
#include "Match/matchChannels.h"
#include "Match/matchConfig.h"
#include "Match/matchEvents.h"
#include "Match/matchRand.h"
#include "Match/matchSession.h"
#include "Match/simClock.h"
#include "Net/lobbyClient.h"
#include "Net/netSession.h"
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

int l_match_clear(lua_State* L)
{
    (void)L;
    MatchConfig::GetInstance().Clear();
    return 0;
}

int l_match_add_keyboard(lua_State* L)
{
    lua_pushboolean(L, MatchConfig::GetInstance().AddKeyboard());
    return 1;
}

int l_match_add_pad(lua_State* L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    const int instance = InputEvent::GetInstance().GetControllerInstanceId(idx);
    lua_pushboolean(L, MatchConfig::GetInstance().AddPad(idx, instance));
    return 1;
}

int l_match_count(lua_State* L)
{
    lua_pushinteger(L, MatchConfig::GetInstance().Count());
    return 1;
}

int l_match_slot(lua_State* L)
{
    int i = (int)luaL_checkinteger(L, 1);
    const MatchSlot* s = MatchConfig::GetInstance().Slot(i);
    if (!s || s->vacant)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 7);
    const char* dev = "keyboard";
    if (s->device == MatchDevice::Pad)
        dev = "pad";
    else if (s->device == MatchDevice::Net)
        dev = "net";
    lua_pushstring(L, dev);
    lua_setfield(L, -2, "device");
    lua_pushinteger(L, s->pad_index);
    lua_setfield(L, -2, "pad");
    lua_pushinteger(L, s->pad_instance);
    lua_setfield(L, -2, "pad_instance");
    lua_pushinteger(L, s->r);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, s->g);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, s->b);
    lua_setfield(L, -2, "b");
    lua_pushinteger(L, s->a);
    lua_setfield(L, -2, "a");
    return 1;
}

int l_match_has_keyboard(lua_State* L)
{
    lua_pushboolean(L, MatchConfig::GetInstance().HasKeyboard());
    return 1;
}

int l_match_has_pad(lua_State* L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, MatchConfig::GetInstance().HasPad(idx));
    return 1;
}

int l_match_is_host(lua_State* L)
{
    lua_pushboolean(L, MatchSession::GetInstance().IsHost() ? 1 : 0);
    return 1;
}

int l_match_is_client(lua_State* L)
{
    lua_pushboolean(L, MatchSession::GetInstance().IsClient() ? 1 : 0);
    return 1;
}

int l_match_is_net_server(lua_State* L)
{
    lua_pushboolean(L, MatchSession::GetInstance().IsNetServer() ? 1 : 0);
    return 1;
}

int l_match_local_slot(lua_State* L)
{
    lua_pushinteger(L, MatchSession::GetInstance().LocalControlledSlot());
    return 1;
}

int l_match_content_hash(lua_State* L)
{
    lua_pushstring(L, MatchSession::GetInstance().ContentHashHex().c_str());
    return 1;
}

int l_match_force_snap(lua_State* L)
{
    int slot = (int)luaL_checkinteger(L, 1);
    MatchSession::GetInstance().RequestForceSnap(slot);
    return 0;
}

int l_input_frame(lua_State* L)
{
    int slot = (int)luaL_optinteger(L, 1, 0);
    const InputFrame& f = InputSampler::GetInstance().Frame(slot);
    lua_createtable(L, 0, 16);
    lua_pushinteger(L, (lua_Integer)f.tick);
    lua_setfield(L, -2, "tick");
    lua_pushinteger(L, f.slot);
    lua_setfield(L, -2, "slot");
    auto pushb = [&](const char* name, uint32_t bit) {
        lua_pushboolean(L, f.Has(bit) ? 1 : 0);
        lua_setfield(L, -2, name);
    };
    pushb("left", InputBits::Left);
    pushb("right", InputBits::Right);
    pushb("up", InputBits::Up);
    pushb("down", InputBits::Down);
    pushb("btn0_held", InputBits::Btn0Held);
    pushb("btn0_pressed", InputBits::Btn0Pressed);
    pushb("btn1_held", InputBits::Btn1Held);
    pushb("btn1_pressed", InputBits::Btn1Pressed);
    pushb("btn2_pressed", InputBits::Btn2Pressed);
    pushb("btn3_held", InputBits::Btn3Held);
    pushb("btn4_held", InputBits::Btn4Held);
    pushb("btn5_held", InputBits::Btn5Held);
    lua_pushnumber(L, f.axis_x);
    lua_setfield(L, -2, "axis_x");
    lua_pushnumber(L, f.axis_y);
    lua_setfield(L, -2, "axis_y");
    return 1;
}

int l_channel_set_int(lua_State* L)
{
    if (!MatchSession::GetInstance().IsHost())
        return 0;
    const char* key = luaL_checkstring(L, 1);
    int v = (int)luaL_checkinteger(L, 2);
    MatchChannels::GetInstance().SetInt(key, v);
    return 0;
}

int l_channel_get_int(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    int fallback = (int)luaL_optinteger(L, 2, 0);
    lua_pushinteger(L, MatchChannels::GetInstance().GetInt(key, fallback));
    return 1;
}

int l_rand(lua_State* L)
{
    lua_pushnumber(L, MatchRand::GetInstance().NextFloat());
    return 1;
}

int l_rand_range(lua_State* L)
{
    int lo = (int)luaL_checkinteger(L, 1);
    int hi = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, MatchRand::GetInstance().NextRange(lo, hi));
    return 1;
}

int l_net_emit_sound(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    int slot = (int)luaL_optinteger(L, 3, -1);
    const bool has_xy = lua_gettop(L) >= 5;
    float x = has_xy ? (float)luaL_checknumber(L, 4) : 0.f;
    float y = has_xy ? (float)luaL_checknumber(L, 5) : 0.f;
    if (!MatchSession::GetInstance().IsHost())
        return 0;
    const uint8_t flags = has_xy ? MatchEventFlags::Spatial : MatchEventFlags::None;
    MatchEvents::GetInstance().Emit(MatchEventKind::Sound, path, vol, slot, x, y,
                                    SimClock::GetInstance().Tick(), flags);
    return 0;
}

int l_net_emit_cue(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    int slot = (int)luaL_optinteger(L, 2, -1);
    float vol = (float)luaL_optnumber(L, 3, 1.0);
    const bool has_xy = lua_gettop(L) >= 5;
    float x = has_xy ? (float)luaL_checknumber(L, 4) : 0.f;
    float y = has_xy ? (float)luaL_checknumber(L, 5) : 0.f;
    if (!MatchSession::GetInstance().IsHost())
        return 0;
    const uint8_t flags = has_xy ? MatchEventFlags::Spatial : MatchEventFlags::None;
    MatchEvents::GetInstance().Emit(MatchEventKind::Cue, name, vol, slot, x, y,
                                    SimClock::GetInstance().Tick(), flags);
    return 0;
}

int l_net_poll_event(lua_State* L)
{
    if (g_script_events.empty())
    {
        lua_pushnil(L);
        return 1;
    }
    MatchEventWire ev = g_script_events.front();
    g_script_events.erase(g_script_events.begin());
    lua_createtable(L, 0, 7);
    lua_pushinteger(L, ev.kind);
    lua_setfield(L, -2, "kind");
    lua_pushstring(L, ev.name);
    lua_setfield(L, -2, "name");
    lua_pushnumber(L, ev.volume);
    lua_setfield(L, -2, "volume");
    lua_pushinteger(L, ev.slot);
    lua_setfield(L, -2, "slot");
    lua_pushnumber(L, ev.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, ev.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, ev.flags);
    lua_setfield(L, -2, "flags");
    return 1;
}

int l_net_host_start(lua_State* L)
{
    int port = (int)luaL_optinteger(L, 1, NetMsg::kDefaultPort);
    bool ok = NetHost::GetInstance().Listen((uint16_t)port);
    if (ok)
    {
        MatchSession::GetInstance().SetAuthority(MatchAuthority::NetHost);
        MatchSession::GetInstance().SetSeed((uint64_t)SDL_GetTicks() ^ 0xC0FFEEULL);
    }
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_net_host_stop(lua_State* L)
{
    (void)L;
    LobbyClient::GetInstance().CancelOp();
    NetHost::GetInstance().Close();
    LobbyClient::GetInstance().CloseRoom();
    LobbyClient::GetInstance().Close();
    MatchSession::GetInstance().ResetForLocal();
    return 0;
}

int l_net_host_start_dedicated(lua_State* L)
{
    int port = (int)luaL_optinteger(L, 1, NetMsg::kDefaultPort);
    bool ok = NetHost::GetInstance().Listen((uint16_t)port);
    if (ok)
    {
        // Same TCP listen path; authority marks headless so game can skip local pawn.
        MatchSession::GetInstance().SetAuthority(MatchAuthority::DedicatedHost);
        MatchSession::GetInstance().SetSeed((uint64_t)SDL_GetTicks() ^ 0xDED1CA7EULL);
        MatchSession::GetInstance().SetLocalControlledSlot(-1);
    }
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_net_host_attach_relay(lua_State* L)
{
    (void)L;
    if (!LobbyClient::GetInstance().HasRoom())
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    bool ok = NetHost::GetInstance().BeginAttachRelay(LobbyClient::GetInstance().LastRoom());
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_net_host_attach_relay_poll(lua_State* L)
{
    NetHost::GetInstance().Poll();
    lua_pushstring(L, NetHost::GetInstance().AttachRelayStatusName());
    return 1;
}

int l_net_host_attach_relay_error(lua_State* L)
{
    lua_pushstring(L, NetHost::GetInstance().AttachRelayError().c_str());
    return 1;
}

int l_lobby_set_endpoint(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    int port = (int)luaL_optinteger(L, 2, LobbyProto::kDefaultLobbyPort);
    LobbyClient::GetInstance().SetLobbyEndpoint(host, (uint16_t)port);
    return 0;
}

int l_lobby_create_room(lua_State* L)
{
    // Async begin - poll with lobby_op_poll(); room code via lobby_room_code when ready.
    LobbyProto::CreateRoomReq req{};
    req.proto = LobbyProto::kProto;
    req.kind = LobbyProto::ListenRelay;
    req.content_hash = MatchSession::GetInstance().ContentHashValue();
    req.game_port = (uint16_t)luaL_optinteger(L, 1, NetMsg::kDefaultPort);
    if (lua_isstring(L, 2))
        std::strncpy(req.advertise_host, lua_tostring(L, 2), LobbyProto::kHostBuf - 1);
    bool ok = LobbyClient::GetInstance().BeginCreateRoom(req);
    if (!ok)
    {
        lua_pushnil(L);
        lua_pushstring(L, "async lobby submit failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_pushnil(L);
    return 2;
}

int l_lobby_join_room(lua_State* L)
{
    const char* code = luaL_checkstring(L, 1);
    bool ok = LobbyClient::GetInstance().BeginJoinRoom(
        code, MatchSession::GetInstance().ContentHashValue());
    if (!ok)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "async lobby submit failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_pushnil(L);
    return 2;
}

int l_lobby_op_poll(lua_State* L)
{
    // poll in case FixedUpdate skipped LobbyClient::Poll this frame
    LobbyClient::GetInstance().Poll();
    lua_pushstring(L, LobbyClient::GetInstance().OpStatusName());
    return 1;
}

int l_lobby_op_error(lua_State* L)
{
    lua_pushstring(L, LobbyClient::GetInstance().OpError().c_str());
    return 1;
}

int l_lobby_op_cancel(lua_State* L)
{
    (void)L;
    LobbyClient::GetInstance().CancelOp();
    return 0;
}

int l_lobby_register_dedicated(lua_State* L)
{
    LobbyProto::RegisterDedicatedReq req{};
    req.proto = LobbyProto::kProto;
    req.content_hash = MatchSession::GetInstance().ContentHashValue();
    req.game_port = (uint16_t)luaL_optinteger(L, 1, NetMsg::kDefaultPort);
    if (lua_isstring(L, 2))
        std::strncpy(req.advertise_host, lua_tostring(L, 2), LobbyProto::kHostBuf - 1);
    LobbyProto::RoomInfoMsg info{};
    std::string err;
    if (!LobbyClient::GetInstance().RegisterDedicated(req, info, err))
    {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }
    lua_pushstring(L, info.code);
    lua_pushnil(L);
    return 2;
}

int l_lobby_room_code(lua_State* L)
{
    if (!LobbyClient::GetInstance().HasRoom())
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, LobbyClient::GetInstance().LastRoom().code);
    return 1;
}

int l_net_join_from_lobby(lua_State* L)
{
    bool ok = NetClient::GetInstance().BeginJoinFromLobby();
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_net_client_count(lua_State* L)
{
    lua_pushinteger(L, NetHost::GetInstance().ClientCount());
    return 1;
}

int l_net_join(lua_State* L)
{
    // Async begin - use net_join_poll() for status (Hello sent on ready).
    const char* host = luaL_checkstring(L, 1);
    int port = (int)luaL_optinteger(L, 2, NetMsg::kDefaultPort);
    bool ok = NetClient::GetInstance().BeginConnect(host, (uint16_t)port);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_net_join_poll(lua_State* L)
{
    NetClient::GetInstance().Poll();
    lua_pushstring(L, NetClient::GetInstance().ConnectStatusName());
    return 1;
}

int l_net_join_error(lua_State* L)
{
    lua_pushstring(L, NetClient::GetInstance().ConnectError().c_str());
    return 1;
}

int l_net_join_cancel(lua_State* L)
{
    (void)L;
    NetClient::GetInstance().CancelConnect();
    return 0;
}

int l_net_client_ready(lua_State* L)
{
    auto& c = NetClient::GetInstance();
    lua_pushboolean(L, c.HasWelcome() ? 1 : 0);
    return 1;
}

int l_net_client_hash_rejected(lua_State* L)
{
    lua_pushboolean(L, NetClient::GetInstance().HashRejected() ? 1 : 0);
    return 1;
}

int l_net_client_start_pending(lua_State* L)
{
    auto& c = NetClient::GetInstance();
    bool s = c.StartRequested();
    if (s)
        c.ClearStart();
    lua_pushboolean(L, s ? 1 : 0);
    return 1;
}

int l_net_client_close(lua_State* L)
{
    (void)L;
    NetClient::GetInstance().Close();
    MatchSession::GetInstance().ResetForLocal();
    return 0;
}

int l_net_broadcast_start(lua_State* L)
{
    (void)L;
    NetHost::GetInstance().Broadcast(NetMsg::StartMatch, nullptr, 0);
    return 0;
}

int l_pad_any_input(lua_State* L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, InputEvent::GetInstance().AnyControllerInputThisFrame(idx));
    return 1;
}

int l_pad_instance_id(lua_State* L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, InputEvent::GetInstance().GetControllerInstanceId(idx));
    return 1;
}

int l_pad_index_of(lua_State* L)
{
    int instance = (int)luaL_checkinteger(L, 1);
    const int idx = InputEvent::GetInstance().GetControllerIndexFromInstance(instance);
    if (idx < 0)
        lua_pushnil(L);
    else
        lua_pushinteger(L, idx);
    return 1;
}

int l_get_player_input(lua_State* L)
{
    entt::entity e = ResolveEntityArg(L, 1);
    if (e == entt::null || !g_registry || !g_registry->has<PlayerInputComponent>(e))
    {
        lua_pushinteger(L, 0); // slot
        lua_pushstring(L, "keyboard");
        lua_pushinteger(L, -1);
        lua_pushinteger(L, -1);
        return 4;
    }
    auto& pin = g_registry->get<PlayerInputComponent>(e);
    lua_pushinteger(L, pin.slot);
    const char* dev = "keyboard";
    if (pin.device == 1)
        dev = "pad";
    else if (pin.device == 2)
        dev = "net";
    lua_pushstring(L, dev);
    lua_pushinteger(L, pin.pad_index);
    lua_pushinteger(L, pin.pad_instance);
    return 4;
}

int l_player_count(lua_State* L)
{
    lua_pushinteger(L, MatchConfig::GetInstance().Count() > 0
                           ? MatchConfig::GetInstance().Count()
                           : 1);
    return 1;
}

int l_player_tag(lua_State* L)
{
    int slot = (int)luaL_checkinteger(L, 1);
    if (slot <= 0)
        lua_pushstring(L, "player");
    else
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "player_%d", slot);
        lua_pushstring(L, buf);
    }
    return 1;
}

void RegisterMatchBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(match_clear, l_match_clear);
    BIND(match_add_keyboard, l_match_add_keyboard);
    BIND(match_add_pad, l_match_add_pad);
    BIND(match_count, l_match_count);
    BIND(match_slot, l_match_slot);
    BIND(match_has_keyboard, l_match_has_keyboard);
    BIND(match_has_pad, l_match_has_pad);
    BIND(match_is_host, l_match_is_host);
    BIND(match_is_client, l_match_is_client);
    BIND(match_is_net_server, l_match_is_net_server);
    BIND(match_local_slot, l_match_local_slot);
    BIND(match_content_hash, l_match_content_hash);
    BIND(match_force_snap, l_match_force_snap);
    BIND(input_frame, l_input_frame);
    BIND(channel_set_int, l_channel_set_int);
    BIND(channel_get_int, l_channel_get_int);
    BIND(rand, l_rand);
    BIND(rand_range, l_rand_range);
    BIND(net_emit_sound, l_net_emit_sound);
    BIND(net_emit_cue, l_net_emit_cue);
    BIND(net_poll_event, l_net_poll_event);
    BIND(net_host_start, l_net_host_start);
    BIND(net_host_stop, l_net_host_stop);
    BIND(net_host_start_dedicated, l_net_host_start_dedicated);
    BIND(net_host_attach_relay, l_net_host_attach_relay);
    BIND(net_host_attach_relay_poll, l_net_host_attach_relay_poll);
    BIND(net_host_attach_relay_error, l_net_host_attach_relay_error);
    BIND(lobby_set_endpoint, l_lobby_set_endpoint);
    BIND(lobby_create_room, l_lobby_create_room);
    BIND(lobby_join_room, l_lobby_join_room);
    BIND(lobby_op_poll, l_lobby_op_poll);
    BIND(lobby_op_error, l_lobby_op_error);
    BIND(lobby_op_cancel, l_lobby_op_cancel);
    BIND(lobby_register_dedicated, l_lobby_register_dedicated);
    BIND(lobby_room_code, l_lobby_room_code);
    BIND(net_join_from_lobby, l_net_join_from_lobby);
    BIND(net_client_count, l_net_client_count);
    BIND(net_join, l_net_join);
    BIND(net_join_poll, l_net_join_poll);
    BIND(net_join_error, l_net_join_error);
    BIND(net_join_cancel, l_net_join_cancel);
    BIND(net_client_ready, l_net_client_ready);
    BIND(net_client_hash_rejected, l_net_client_hash_rejected);
    BIND(net_client_start_pending, l_net_client_start_pending);
    BIND(net_client_close, l_net_client_close);
    BIND(net_broadcast_start, l_net_broadcast_start);
    BIND(pad_any_input, l_pad_any_input);
    BIND(pad_instance_id, l_pad_instance_id);
    BIND(pad_index_of, l_pad_index_of);
    BIND(get_player_input, l_get_player_input);
    BIND(player_count, l_player_count);
    BIND(player_tag, l_player_tag);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterMatchBinds(lua_State* L)
{
    RegisterMatchBindsImpl(L);
}

#endif // FW_HAS_LUAU
