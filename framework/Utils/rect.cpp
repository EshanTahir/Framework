#include "Scene/systems.h"
void Rect::Update(RectComponent& rect, float x, float y) { rect.x = x; rect.y = y; }
void Rect::SetTop(entt::registry&, RectComponent& rect, int y) { rect.y = (float)y; }
void Rect::SetLeft(entt::registry&, RectComponent& rect, int x) { rect.x = (float)x; }
void Rect::SetBottom(entt::registry&, RectComponent& rect, int y) { rect.y = (float)y; }
void Rect::SetRight(entt::registry&, RectComponent& rect, int x) { rect.x = (float)x; }
void Rect::SetTopLeft(entt::registry& r, RectComponent& rect, int x, int y) { SetLeft(r, rect, x); SetTop(r, rect, y); }
void Rect::SetBottomLeft(entt::registry& r, RectComponent& rect, int x, int y) { SetLeft(r, rect, x); SetBottom(r, rect, y); }
void Rect::SetTopRight(entt::registry& r, RectComponent& rect, int x, int y) { SetRight(r, rect, x); SetTop(r, rect, y); }
void Rect::SetBottomRight(entt::registry& r, RectComponent& rect, int x, int y) { SetRight(r, rect, x); SetBottom(r, rect, y); }
void Rect::SetMidTop(entt::registry& r, RectComponent& rect, int x, int y) { SetTop(r, rect, y); rect.x = (float)x; }
void Rect::SetMidLeft(entt::registry& r, RectComponent& rect, int x, int y) { SetLeft(r, rect, x); rect.y = (float)y; }
void Rect::SetMidBottom(entt::registry& r, RectComponent& rect, int x, int y) { SetBottom(r, rect, y); rect.x = (float)x; }
void Rect::SetMidRight(entt::registry& r, RectComponent& rect, int x, int y) { SetRight(r, rect, x); rect.y = (float)y; }
void Rect::SetCenter(entt::registry&, RectComponent& rect, int x, int y) { rect.x = (float)x; rect.y = (float)y; }
bool Rect::CollideRect(RectComponent& a, RectComponent& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
