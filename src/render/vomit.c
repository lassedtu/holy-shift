#include "render/vomit.h"

#include <math.h>

#define SPLAT_POINTS 18 // points around the edge of a big splat
#define MAX_STREAKS 9

static const Color VOMIT = {126, 140, 54, 255};
static const Color VOMIT_DARK = {88, 98, 38, 255};
static const Color VOMIT_LIGHT = {170, 174, 84, 255};

static unsigned int Next(unsigned int *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

static float Unit(unsigned int *state)
{
    return (float)(Next(state) % 1000) / 1000.0f;
}

// Fill a shape that goes around a centre, both windings are drawn so the order of the points does not matter
static void FillAround(Vector2 center, const Vector2 *points, int count, Color color)
{
    for (int i = 0; i < count; i++)
    {
        Vector2 a = points[i];
        Vector2 b = points[(i + 1) % count];
        DrawTriangle(center, a, b, color);
        DrawTriangle(center, b, a, color);
    }
}

static void DrawSplat(Vector2 c, float r, unsigned int seed, float alpha)
{
    Color fill = Fade(VOMIT, alpha);
    Color dark = Fade(VOMIT_DARK, alpha);
    unsigned int state = seed;
    Vector2 edge[SPLAT_POINTS];
    Vector2 outline[SPLAT_POINTS];
    Vector2 drops[SPLAT_POINTS];
    float drop_size[SPLAT_POINTS];
    int drop_count = 0;
    float line = fmaxf(r * 0.04f, 1.5f);
    for (int i = 0; i < SPLAT_POINTS; i++)
    {
        float angle = (float)i / SPLAT_POINTS * 2.0f * PI + (Unit(&state) - 0.5f) * 0.25f;
        float out = r * (0.72f + 0.3f * Unit(&state));
        if (Unit(&state) < 0.3f)
        {
            // An arm: a point that sticks out, with a drop at its end
            out = r * (1.1f + 0.3f * Unit(&state));
            float size = r * (0.1f + 0.06f * Unit(&state));
            drop_size[drop_count] = size;
            drops[drop_count] = (Vector2){c.x + cosf(angle) * (out + size * 0.2f), c.y + sinf(angle) * (out + size * 0.2f)};
            drop_count++;
        }
        Vector2 direction = {cosf(angle), sinf(angle)};
        edge[i] = (Vector2){c.x + direction.x * out, c.y + direction.y * out};
        outline[i] = (Vector2){edge[i].x + direction.x * line, edge[i].y + direction.y * line};
    }

    // Streaks run down the glass from the lower half, with clear glass between them
    int streaks = 5 + (int)(Next(&state) % 4);
    float streak_x[MAX_STREAKS];
    float streak_end[MAX_STREAKS];
    float streak_width[MAX_STREAKS];
    for (int i = 0; i < streaks; i++)
    {
        streak_x[i] = c.x + ((float)i / (float)(streaks - 1) - 0.5f) * r * 1.6f + (Unit(&state) - 0.5f) * r * 0.15f;
        streak_end[i] = c.y + r * (1.0f + 1.8f * Unit(&state));
        streak_width[i] = r * (0.06f + 0.1f * Unit(&state));
        float w = streak_width[i] + line;
        DrawRectangleRec((Rectangle){streak_x[i] - w, c.y, 2.0f * w, streak_end[i] - c.y}, dark);
        DrawCircleV((Vector2){streak_x[i], streak_end[i]}, w * 1.3f, dark);
    }
    FillAround(c, outline, SPLAT_POINTS, dark);
    for (int i = 0; i < drop_count; i++)
    {
        DrawCircleV(drops[i], drop_size[i] + line, dark);
    }
    FillAround(c, edge, SPLAT_POINTS, fill);
    for (int i = 0; i < drop_count; i++)
    {
        DrawCircleV(drops[i], drop_size[i], fill);
    }
    for (int i = 0; i < streaks; i++)
    {
        float w = streak_width[i];
        DrawRectangleRec((Rectangle){streak_x[i] - w, c.y, 2.0f * w, streak_end[i] - c.y}, fill);
        DrawCircleV((Vector2){streak_x[i], streak_end[i]}, w * 1.3f, fill);
    }

    // Lumps in the splat, and a shine at its top left
    for (int i = 0; i < 4; i++)
    {
        float angle = Unit(&state) * 2.0f * PI;
        float out = r * 0.45f * Unit(&state);
        DrawCircleV((Vector2){c.x + cosf(angle) * out, c.y + sinf(angle) * out}, r * (0.07f + 0.05f * Unit(&state)),
                    dark);
    }
    DrawCircleV((Vector2){c.x - r * 0.35f, c.y - r * 0.35f}, r * 0.2f, Fade(VOMIT_LIGHT, alpha));
    DrawCircleV((Vector2){c.x - r * 0.12f, c.y - r * 0.5f}, r * 0.08f, Fade(VOMIT_LIGHT, alpha));
}

static void DrawDrop(Vector2 c, float r, float alpha)
{
    float line = fmaxf(r * 0.15f, 1.5f);
    DrawCircleV(c, r + line, Fade(VOMIT_DARK, alpha));
    DrawCircleV(c, r, Fade(VOMIT, alpha));
    DrawCircleV((Vector2){c.x - r * 0.35f, c.y - r * 0.35f}, fmaxf(r * 0.3f, 1.0f), Fade(VOMIT_LIGHT, alpha));
}

void VomitDraw(Vector2 center, float radius, unsigned int seed, bool big, float alpha)
{
    if (big)
    {
        DrawSplat(center, radius, seed, alpha);
    }
    else
    {
        DrawDrop(center, radius, alpha);
    }
}
