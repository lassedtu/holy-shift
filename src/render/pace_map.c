#include "render/pace_map.h"

#include <math.h>

#include "core/config.h"
#include "raymath.h"
#include "rlgl.h"
#include "util/math_util.h"

#define PAPER_WIDTH 152 // texture pixels
#define PAPER_HEIGHT 224
#define NOTE_HEIGHT 76    // height of the text area at the bottom of the paper
#define MAP_SCALE 0.17f   // paper pixels for each map pixel
#define CAR_Y 128.0f      // the car is this far down on the paper
#define DRAW_BEHIND 80.0f // the road is drawn this far behind the car, in map pixels
#define DRAW_AHEAD 900.0f // and this far ahead of the car
#define DRAW_STEP 8.0f
#define ROAD_MAX_POINTS ((int)((DRAW_BEHIND + DRAW_AHEAD) / DRAW_STEP) + 2)
#define NEAREST_WINDOW 120.0f // the car cannot move more than this along the track in one frame
#define HEADING_AHEAD 8.0f    // the top of the map points to the road this far ahead of the car
#define HEADING_RATE 12.0f    // how fast the map turns to the road, only to smooth small steps
#define INTO_DISTANCE 240.0f  // a corner that starts this near after the next corner is called with it

// Where the map is held, as seen from the eye of the driver, in meters and degrees
#define HOLD_YAW_DEG 86.0f
#define HOLD_ELEVATION_DEG -20.0f
#define HOLD_DISTANCE 0.55f
#define HOLD_WIDTH 0.40f
#define HOLD_ROLL_DEG -6.0f // the top of the map leans towards the driver
#define SWAY_ROLL_DEG 1.2f
#define SWAY_BOB 0.004f
#define VISIBLE_MARGIN_DEG 20.0f // the map is drawn when the head yaw is this near to the edge of the view

#define COLOR_PAPER ((Color){226, 214, 184, 255})
#define COLOR_PAPER_DARK ((Color){196, 182, 150, 255})
#define COLOR_INK ((Color){48, 36, 28, 255})
#define COLOR_ROAD_INK ((Color){166, 120, 78, 255})
#define COLOR_RED_INK ((Color){176, 34, 30, 255})
#define COLOR_CAR ((Color){30, 70, 170, 255})
#define COLOR_CABIN_LIGHT ((Color){200, 196, 186, 255}) // the map is in the shade of the cabin

bool PaceMapInit(PaceMap *map)
{
    *map = (PaceMap){0};
    map->paper = LoadRenderTexture(PAPER_WIDTH, PAPER_HEIGHT);
    map->view = LoadRenderTexture(VIEW_WIDTH, VIEW_HEIGHT);
    if (!IsRenderTextureValid(map->paper) || !IsRenderTextureValid(map->view))
    {
        PaceMapUnload(map);
        return false;
    }
    SetTextureFilter(map->paper.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(map->view.texture, TEXTURE_FILTER_POINT);
    return true;
}

void PaceMapUnload(PaceMap *map)
{
    if (IsRenderTextureValid(map->view))
    {
        UnloadRenderTexture(map->view);
    }
    if (IsRenderTextureValid(map->paper))
    {
        UnloadRenderTexture(map->paper);
    }
    *map = (PaceMap){0};
}

void PaceMapReset(PaceMap *map, const Track *track, const Car *car)
{
    map->position = TrackNearest(track, car->position, 0.0f, track->length / 2.0f);
    map->heading = TrackHeading(track, map->position + HEADING_AHEAD);
}

void PaceMapUpdate(PaceMap *map, const Track *track, const Car *car, float dt)
{
    map->time += dt;
    map->position = TrackNearest(track, car->position, map->position, NEAREST_WINDOW);
    float target = TrackHeading(track, map->position + HEADING_AHEAD);
    map->heading += AngleDifference(map->heading, target) * fminf(1.0f, HEADING_RATE * dt);
}

// Return the paper position of a map position
static Vector2 ToPaper(const PaceMap *map, Vector2 center, Vector2 point)
{
    Vector2 v = Vector2Subtract(point, center);
    Vector2 forward = {cosf(map->heading), sinf(map->heading)};
    Vector2 right = {-forward.y, forward.x};
    return (Vector2){PAPER_WIDTH / 2.0f + Vector2DotProduct(v, right) * MAP_SCALE,
                     CAR_Y - Vector2DotProduct(v, forward) * MAP_SCALE};
}

static void DrawRoad(const PaceMap *map, const Track *track, Vector2 center)
{
    float from = map->position - DRAW_BEHIND;
    float to = map->position + DRAW_AHEAD;
    if (!track->closed)
    {
        from = fmaxf(from, 0.0f);
        to = fminf(to, track->length);
    }
    Vector2 points[ROAD_MAX_POINTS];
    int count = 0;
    for (float s = from; s <= to && count < ROAD_MAX_POINTS; s += DRAW_STEP)
    {
        points[count++] = ToPaper(map, center, TrackPoint(track, s));
    }
    if (count >= 2)
    {
        DrawSplineLinear(points, count, 5.0f, COLOR_INK);
        DrawSplineLinear(points, count, 2.0f, COLOR_ROAD_INK);
    }
}

static bool IsSlow(const PaceCorner *corner)
{
    return corner->grade <= 2;
}

static void DrawCornerNotes(const PaceMap *map, const Track *track, const PaceNotes *notes,
                            Vector2 center, int next)
{
    for (int i = 0; i < notes->count; i++)
    {
        const PaceCorner *corner = &notes->corners[i];
        float ahead = corner->apex - map->position;
        if (track->closed && ahead < -track->length / 2.0f)
        {
            ahead += track->length;
        }
        if (ahead < -DRAW_BEHIND || ahead > DRAW_AHEAD)
        {
            continue;
        }

        // The note is at the outside of the corner
        float heading = TrackHeading(track, corner->apex);
        Vector2 outside = {sinf(heading) * (float)corner->direction, -cosf(heading) * (float)corner->direction};
        Vector2 apex = ToPaper(map, center, TrackPoint(track, corner->apex));
        Vector2 offset = ToPaper(map, center, Vector2Add(TrackPoint(track, corner->apex), outside));
        Vector2 direction = Vector2Normalize(Vector2Subtract(offset, apex));
        Vector2 at = Vector2Add(apex, Vector2Scale(direction, 14.0f));

        char text[8];
        PaceShortCall(corner, text, sizeof(text));
        int width = MeasureText(text, 10);
        int x = (int)Clamp(at.x - (float)width / 2.0f, 3.0f, (float)(PAPER_WIDTH - width - 3));
        int y = (int)(at.y - 5.0f);
        Color ink = IsSlow(corner) ? COLOR_RED_INK : COLOR_INK;
        if (i == next)
        {
            DrawRectangle(x - 2, y - 2, width + 4, 13, COLOR_PAPER);
            DrawRectangleLines(x - 2, y - 2, width + 4, 13, ink);
        }
        DrawText(text, x, y, 10, ink);
    }
}

static void DrawCar(const PaceMap *map, const Car *car, Vector2 center)
{
    Vector2 at = ToPaper(map, center, car->position);
    float angle = car->heading - map->heading; // 0 points to the top of the paper
    Vector2 forward = {sinf(angle), -cosf(angle)};
    Vector2 right = {-forward.y, forward.x};
    Vector2 tip = Vector2Add(at, Vector2Scale(forward, 5.0f));
    Vector2 left_back = Vector2Subtract(Vector2Subtract(at, Vector2Scale(forward, 3.0f)), Vector2Scale(right, 3.5f));
    Vector2 right_back = Vector2Add(Vector2Subtract(at, Vector2Scale(forward, 3.0f)), Vector2Scale(right, 3.5f));
    DrawTriangle(tip, left_back, right_back, COLOR_CAR);
    DrawTriangle(tip, right_back, left_back, COLOR_CAR);
}

static void DrawCentered(const char *text, int y, int size, Color color)
{
    DrawText(text, (PAPER_WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

static void DrawNextCorner(const Track *track, const PaceNotes *notes, int next,
                           float distance)
{
    int top = PAPER_HEIGHT - NOTE_HEIGHT;
    DrawRectangle(0, top, PAPER_WIDTH, NOTE_HEIGHT, COLOR_PAPER);
    DrawLine(4, top, PAPER_WIDTH - 4, top, COLOR_INK);
    if (next < 0)
    {
        DrawCentered("FINISH", top + 26, 20, COLOR_INK);
        return;
    }

    const PaceCorner *corner = &notes->corners[next];
    Color ink = IsSlow(corner) ? COLOR_RED_INK : COLOR_INK;
    DrawCentered(PaceGradeWord(corner), top + 5, 20, ink);
    DrawCentered(PaceSideWord(corner), top + 26, 20, ink);

    int meters = (int)(distance / MAP_PIXELS_PER_METER / 10.0f + 0.5f) * 10;
    DrawCentered(TextFormat("in %d m", meters < 10 ? 10 : meters), top + 49, 10, COLOR_INK);

    int following = next + 1 < notes->count ? next + 1 : (track->closed ? 0 : -1);
    float gap = following >= 0 ? notes->corners[following].entry - corner->end : INFINITY;
    if (track->closed && gap < 0.0f)
    {
        gap += track->length;
    }
    if (following >= 0 && following != next && gap < INTO_DISTANCE)
    {
        char into[32];
        PaceCall(&notes->corners[following], into, sizeof(into));
        DrawCentered(TextFormat("into %s", into), top + 61, 10, COLOR_INK);
    }
}

static void RenderPaper(PaceMap *map, const Track *track, const PaceNotes *notes, const Car *car)
{
    Vector2 center = TrackPoint(track, map->position);
    float distance;
    int next = PaceNextCorner(notes, track, map->position, &distance);

    BeginTextureMode(map->paper);
    ClearBackground(COLOR_PAPER);
    DrawLine(PAPER_WIDTH / 2, 0, PAPER_WIDTH / 2, PAPER_HEIGHT, COLOR_PAPER_DARK); // the fold
    DrawLine(0, PAPER_HEIGHT / 3, PAPER_WIDTH, PAPER_HEIGHT / 3, COLOR_PAPER_DARK);
    DrawRoad(map, track, center);
    DrawCornerNotes(map, track, notes, center, next);
    DrawCar(map, car, center);
    DrawNextCorner(track, notes, next, distance);
    DrawRectangleLines(0, 0, PAPER_WIDTH, PAPER_HEIGHT, COLOR_PAPER_DARK);
    EndTextureMode();
}

// Return the direction of a yaw and an elevation in degrees, in the space of the 3D camera
// The camera looks along +z, and +x is to the left in that space
static Vector3 Direction(float yaw_deg, float elevation_deg)
{
    float yaw = yaw_deg * DEG2RAD;
    float elevation = elevation_deg * DEG2RAD;
    return (Vector3){-sinf(yaw) * cosf(elevation), sinf(elevation), cosf(yaw) * cosf(elevation)};
}

static void RenderView(PaceMap *map, float head_yaw, float head_pitch)
{
    float aspect = (float)VIEW_HEIGHT / VIEW_WIDTH;
    Camera3D camera = {
        .position = {0.0f, 0.0f, 0.0f},
        .target = Direction(head_yaw, -head_pitch),
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 2.0f * atanf(tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD) * aspect) * RAD2DEG,
        .projection = CAMERA_PERSPECTIVE,
    };

    Vector3 forward = Direction(HOLD_YAW_DEG, HOLD_ELEVATION_DEG);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0.0f, 1.0f, 0.0f}));
    Vector3 up = Vector3CrossProduct(right, forward);
    float roll = (HOLD_ROLL_DEG + SWAY_ROLL_DEG * sinf(map->time * 1.3f)) * DEG2RAD;
    right = Vector3RotateByAxisAngle(right, forward, roll);
    up = Vector3RotateByAxisAngle(up, forward, roll);

    float half_width = HOLD_WIDTH / 2.0f;
    float half_height = half_width * PAPER_HEIGHT / PAPER_WIDTH;
    Vector3 center = Vector3Scale(forward, HOLD_DISTANCE);
    center = Vector3Add(center, Vector3Scale(up, SWAY_BOB * sinf(map->time * 2.1f)));
    Vector3 r = Vector3Scale(right, half_width);
    Vector3 u = Vector3Scale(up, half_height);
    Vector3 top_left = Vector3Subtract(Vector3Add(center, u), r);
    Vector3 top_right = Vector3Add(Vector3Add(center, u), r);
    Vector3 bottom_left = Vector3Subtract(Vector3Subtract(center, u), r);
    Vector3 bottom_right = Vector3Add(Vector3Subtract(center, u), r);

    BeginTextureMode(map->view);
    ClearBackground(BLANK);
    BeginMode3D(camera);
    rlDisableBackfaceCulling();
    // A render texture is upside down, so the top of the paper is at v = 1
    rlSetTexture(map->paper.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(COLOR_CABIN_LIGHT.r, COLOR_CABIN_LIGHT.g, COLOR_CABIN_LIGHT.b, 255);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(top_left.x, top_left.y, top_left.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(bottom_left.x, bottom_left.y, bottom_left.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(bottom_right.x, bottom_right.y, bottom_right.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(top_right.x, top_right.y, top_right.z);
    rlEnd();
    rlSetTexture(0);
    rlEnableBackfaceCulling();
    EndMode3D();
    EndTextureMode();
}

void PaceMapRender(PaceMap *map, const Track *track, const PaceNotes *notes, const Car *car,
                   float head_yaw, float head_pitch)
{
    map->visible = fabsf(head_yaw - HOLD_YAW_DEG) < VIEW_HFOV_DEG / 2.0f + VISIBLE_MARGIN_DEG;
    if (!map->visible)
    {
        return;
    }
    RenderPaper(map, track, notes, car);
    RenderView(map, head_yaw, head_pitch);
}

void PaceMapDraw(const PaceMap *map, Rectangle destination)
{
    if (!map->visible)
    {
        return;
    }
    Rectangle source = {0, 0, VIEW_WIDTH, -VIEW_HEIGHT};
    DrawTexturePro(map->view.texture, source, destination, (Vector2){0, 0}, 0.0f, WHITE);
}
