#pragma once

// Engine key / mouse abstraction over SDL scancodes
enum class Key
{
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Left, Right, Up, Down,
    Space, LShift, RShift, LCtrl, RCtrl, Escape, Return, Tab,
    Count
};

enum class MouseButton
{
    Left = 1,
    Middle = 2,
    Right = 3
};

int KeyToScancode(Key key);
Key ScancodeToKey(int scancode);
