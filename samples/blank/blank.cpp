// Minimal host: proves Framework links with zero game (src/) code.
#include "framework.h"
#include "Core/appIdentity.h"
#include "Core/application.h"
#include "Core/log.h"

#include "SDL.h"

void GameParseArgs(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    AppIdentity::Set("Blank", "Framework", "Blank");
}

void GameRegisterContent()
{
}

static void BlankStart()
{
    scene_handler.AddScene("Blank");
    scene_handler.SetActiveScene("Blank");
    FW_CORE_INFO("BlankHost start");
}

static void BlankUpdate(float dt)
{
    (void)dt;
    if (InputEvent::GetInstance().KeyDownThisFrame((SDL_Keycode)SDL_SCANCODE_ESCAPE))
        AppEvent::GetInstance().RequestQuit();
}

static void BlankEnd()
{
    FW_CORE_INFO("BlankHost end");
}

Application* CreateApplication()
{
    return new Application(BlankStart, BlankUpdate, BlankEnd);
}
