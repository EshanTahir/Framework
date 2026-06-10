#include "appIdentity.h"

#include <string>

namespace
{
    std::string g_title = "Game";
    std::string g_org = "Game";
    std::string g_app = "Game";
}

namespace AppIdentity
{
    void Set(const char* window_title, const char* org, const char* app)
    {
        if (window_title && window_title[0])
            g_title = window_title;
        if (org && org[0])
            g_org = org;
        if (app && app[0])
            g_app = app;
    }

    const char* WindowTitle() { return g_title.c_str(); }
    const char* Org() { return g_org.c_str(); }
    const char* App() { return g_app.c_str(); }
}
