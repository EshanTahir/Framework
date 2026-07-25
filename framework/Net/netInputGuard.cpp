#include "netInputGuard.h"

#include <cmath>

bool NetInputGuard::Accept(ClientState& st, const InputFrame& f, int /*assigned_slot*/, uint64_t host_tick,
                           uint64_t now_ms, bool* kick, std::string* reason)
{
    auto fail = [&](const char* why) {
        st.strikes++;
        if (reason)
            *reason = why;
        if (kick)
            *kick = st.strikes >= 8;
        return false;
    };

    if (kick)
        *kick = false;

    if ((f.bits & ~kAllowedBits) != 0)
        return fail("bad bits");

    if (!std::isfinite(f.axis_x) || !std::isfinite(f.axis_y))
        return fail("nan axis");
    if (f.axis_x < -1.5f || f.axis_x > 1.5f || f.axis_y < -1.5f || f.axis_y > 1.5f)
        return fail("axis range");

    if (host_tick > 0 && f.tick > 0)
    {
        const int64_t d = (int64_t)f.tick - (int64_t)host_tick;
        if (d > 120 || d < -300)
            return fail("tick skew");
    }

    if (f.tick > 0 && st.last_tick > 0 && f.tick + 60 < st.last_tick)
        return fail("tick rewind");
    if (f.tick >= st.last_tick)
        st.last_tick = f.tick;

    if (now_ms - st.sec_bucket_ms >= 1000)
    {
        st.sec_bucket_ms = now_ms;
        st.inputs_this_sec = 0;
    }
    st.inputs_this_sec++;
    if (st.inputs_this_sec > 120)
        return fail("input flood");

    if (st.strikes > 0 && (st.inputs_this_sec % 30) == 0)
        st.strikes--;

    return true;
}
