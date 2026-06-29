#include "Scene/systems.h"

void Rect::Update(RectComponent& rect, float x, float y)
{
    rect.x = x;
    rect.y = y;
    rect.left = x;
    rect.top = y;
    rect.right = x + rect.w;
    rect.bottom = y + rect.h;
    rect.centerx = x + rect.w * 0.5f;
    rect.centery = y + rect.h * 0.5f;
    rect.center[0] = rect.centerx;
    rect.center[1] = rect.centery;
}

void Rect::SetTop(entt::registry&, RectComponent& rect, int y)
{
    rect.y = (float)y;
    Update(rect, rect.x, rect.y);
}
void Rect::SetLeft(entt::registry&, RectComponent& rect, int x)
{
    rect.x = (float)x;
    Update(rect, rect.x, rect.y);
}
void Rect::SetBottom(entt::registry&, RectComponent& rect, int y)
{
    rect.y = (float)y - rect.h;
    Update(rect, rect.x, rect.y);
}
void Rect::SetRight(entt::registry&, RectComponent& rect, int x)
{
    rect.x = (float)x - rect.w;
    Update(rect, rect.x, rect.y);
}
void Rect::SetTopLeft(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetLeft(r, rect, x);
    SetTop(r, rect, y);
}
void Rect::SetBottomLeft(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetLeft(r, rect, x);
    SetBottom(r, rect, y);
}
void Rect::SetTopRight(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetRight(r, rect, x);
    SetTop(r, rect, y);
}
void Rect::SetBottomRight(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetRight(r, rect, x);
    SetBottom(r, rect, y);
}
void Rect::SetMidTop(entt::registry& r, RectComponent& rect, int x, int y)
{
    rect.x = (float)x - rect.w * 0.5f;
    SetTop(r, rect, y);
}
void Rect::SetMidLeft(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetLeft(r, rect, x);
    rect.y = (float)y - rect.h * 0.5f;
    Update(rect, rect.x, rect.y);
}
void Rect::SetMidBottom(entt::registry& r, RectComponent& rect, int x, int y)
{
    rect.x = (float)x - rect.w * 0.5f;
    SetBottom(r, rect, y);
}
void Rect::SetMidRight(entt::registry& r, RectComponent& rect, int x, int y)
{
    SetRight(r, rect, x);
    rect.y = (float)y - rect.h * 0.5f;
    Update(rect, rect.x, rect.y);
}
void Rect::SetCenter(entt::registry&, RectComponent& rect, int x, int y)
{
    rect.x = (float)x - rect.w * 0.5f;
    rect.y = (float)y - rect.h * 0.5f;
    Update(rect, rect.x, rect.y);
}

bool Rect::CollideRect(RectComponent& a, RectComponent& b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}
