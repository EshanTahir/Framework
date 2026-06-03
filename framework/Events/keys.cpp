#include "keys.h"
#include "SDL.h"

int KeyToScancode(Key key)
{
    switch (key)
    {
    case Key::A: return SDL_SCANCODE_A;
    case Key::B: return SDL_SCANCODE_B;
    case Key::C: return SDL_SCANCODE_C;
    case Key::D: return SDL_SCANCODE_D;
    case Key::E: return SDL_SCANCODE_E;
    case Key::F: return SDL_SCANCODE_F;
    case Key::G: return SDL_SCANCODE_G;
    case Key::H: return SDL_SCANCODE_H;
    case Key::I: return SDL_SCANCODE_I;
    case Key::J: return SDL_SCANCODE_J;
    case Key::K: return SDL_SCANCODE_K;
    case Key::L: return SDL_SCANCODE_L;
    case Key::M: return SDL_SCANCODE_M;
    case Key::N: return SDL_SCANCODE_N;
    case Key::O: return SDL_SCANCODE_O;
    case Key::P: return SDL_SCANCODE_P;
    case Key::Q: return SDL_SCANCODE_Q;
    case Key::R: return SDL_SCANCODE_R;
    case Key::S: return SDL_SCANCODE_S;
    case Key::T: return SDL_SCANCODE_T;
    case Key::U: return SDL_SCANCODE_U;
    case Key::V: return SDL_SCANCODE_V;
    case Key::W: return SDL_SCANCODE_W;
    case Key::X: return SDL_SCANCODE_X;
    case Key::Y: return SDL_SCANCODE_Y;
    case Key::Z: return SDL_SCANCODE_Z;
    case Key::Left: return SDL_SCANCODE_LEFT;
    case Key::Right: return SDL_SCANCODE_RIGHT;
    case Key::Up: return SDL_SCANCODE_UP;
    case Key::Down: return SDL_SCANCODE_DOWN;
    case Key::Space: return SDL_SCANCODE_SPACE;
    case Key::LShift: return SDL_SCANCODE_LSHIFT;
    case Key::RShift: return SDL_SCANCODE_RSHIFT;
    case Key::LCtrl: return SDL_SCANCODE_LCTRL;
    case Key::RCtrl: return SDL_SCANCODE_RCTRL;
    case Key::Escape: return SDL_SCANCODE_ESCAPE;
    case Key::Return: return SDL_SCANCODE_RETURN;
    case Key::Tab: return SDL_SCANCODE_TAB;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

Key ScancodeToKey(int scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_A: return Key::A;
    case SDL_SCANCODE_D: return Key::D;
    case SDL_SCANCODE_E: return Key::E;
    case SDL_SCANCODE_W: return Key::W;
    case SDL_SCANCODE_S: return Key::S;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;
    case SDL_SCANCODE_UP: return Key::Up;
    case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_RCTRL: return Key::RCtrl;
    case SDL_SCANCODE_LCTRL: return Key::LCtrl;
    case SDL_SCANCODE_K: return Key::K;
    case SDL_SCANCODE_L: return Key::L;
    case SDL_SCANCODE_N: return Key::N;
    case SDL_SCANCODE_M: return Key::M;
    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_LSHIFT: return Key::LShift;
    case SDL_SCANCODE_RSHIFT: return Key::RShift;
    case SDL_SCANCODE_R: return Key::R;
    case SDL_SCANCODE_O: return Key::O;
    case SDL_SCANCODE_TAB: return Key::Tab;
    default: return Key::Unknown;
    }
}
