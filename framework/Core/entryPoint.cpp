#include "application.h"
#include "appIdentity.h"
#include "log.h"
#include "assetCache.h"
#include "paths.h"
#include "glContext.h"
#include "window.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"

int screen_width = 1280;
int screen_height = 720;

extern Application* CreateApplication();
void GameParseArgs(int argc, char** argv);
void GameRegisterContent();

int main(int argc, char* args[])
{
    GameParseArgs(argc, args);
    Log::Init();
    FW_CORE_INFO("Initialized logger");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        FW_CORE_CRITICAL("SDL_Init HAS FAILED. SDL_ERROR: {0}", SDL_GetError());
    if (!(IMG_Init(IMG_INIT_PNG)))
        FW_CORE_CRITICAL("IMG_Init HAS FAILED. SDL_ERROR: {0}", SDL_GetError());
    TTF_Init();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* main_window = SDL_CreateWindow(
        AppIdentity::WindowTitle(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screen_width, screen_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    Window::GetInstance().SetWindow(main_window);
    if (!GlContext::GetInstance().Init(main_window))
        return 1;
    Paths::SetUserDataIdentity(AppIdentity::Org(), AppIdentity::App());
    Paths::Init();
    GameRegisterContent();
    AssetCache::GetInstance().Clear();
    FW_CORE_INFO("Asset root: {0}", Paths::GetAssetRoot());

    auto app = CreateApplication();
    FW_CORE_INFO("Initialized app");
    app->Run();
    delete app;

    AssetCache::GetInstance().Clear();
    Window::GetInstance().CleanUp();
    TTF_Quit();
    SDL_Quit();
    FW_CORE_INFO("Shutdown complete");
    return 0;
}
