#pragma once

#include <string>

// Product-facing strings supplied by the game at boot (not Framework defaults).
namespace AppIdentity
{
    void Set(const char* window_title, const char* org, const char* app);

    const char* WindowTitle();
    const char* Org();
    const char* App();
}
