#include "render/cockpit.h"

#include <math.h>
#include <stdio.h>

#include "core/config.h"
#include "render/shader_util.h"
#include "util/json_lite.h"

#define GLASS_SCALE 0.5f // the picture of the windshield is this part of the size of the panorama

static bool ReadMetrics(CockpitMetrics *metrics, char *message, size_t size)
{
    char *text = FileExists(COCKPIT_METRICS_PATH) ? LoadFileText(COCKPIT_METRICS_PATH) : NULL;
    if (text == NULL)
    {
        snprintf(message, size, "Could not read %s", COCKPIT_METRICS_PATH);
        return false;
    }

    double px_per_deg = 0;
    double yaw_min_deg = 0;
    double lat_top_deg = 0;
    double lat_bottom_deg = 0;
    double wheel_x = 0;
    double wheel_y = 0;
    double frame_width = 0;
    double frame_height = 0;
    bool valid = JsonReadNumber(text, "px_per_deg", &px_per_deg) &&
                 JsonReadNumber(text, "yaw_min_deg", &yaw_min_deg) &&
                 JsonReadNumber(text, "lat_top_deg", &lat_top_deg) &&
                 JsonReadNumber(text, "lat_bottom_deg", &lat_bottom_deg) &&
                 JsonReadNumber(text, "x", &wheel_x) && JsonReadNumber(text, "y", &wheel_y) &&
                 JsonReadNumber(text, "frame_width", &frame_width) &&
                 JsonReadNumber(text, "frame_height", &frame_height);
    int frames =
        JsonReadNumberArray(text, "angles_deg", metrics->wheel_angles_deg, COCKPIT_MAX_WHEEL_FRAMES);
    UnloadFileText(text);
    if (!valid || frames < 1)
    {
        snprintf(message, size, "%s is not a valid cabin file", COCKPIT_METRICS_PATH);
        return false;
    }

    metrics->px_per_deg = (float)px_per_deg;
    metrics->yaw_min_deg = (float)yaw_min_deg;
    metrics->lat_top_deg = (float)lat_top_deg;
    metrics->lat_bottom_deg = (float)lat_bottom_deg;
    metrics->wheel_x = (int)wheel_x;
    metrics->wheel_y = (int)wheel_y;
    metrics->wheel_frame_width = (int)frame_width;
    metrics->wheel_frame_height = (int)frame_height;
    metrics->wheel_frame_count = frames;
    return true;
}

bool CockpitLoad(Cockpit *cockpit, char *message, size_t size)
{
    *cockpit = (Cockpit){0};
    if (!ReadMetrics(&cockpit->metrics, message, size))
    {
        return false;
    }

    cockpit->panorama = LoadTexture(COCKPIT_PANORAMA_PATH);
    cockpit->wheel = LoadTexture(COCKPIT_WHEEL_PATH);
    if (!IsTextureValid(cockpit->panorama) || !IsTextureValid(cockpit->wheel))
    {
        snprintf(message, size, "Could not load the cabin images from assets/");
        CockpitUnload(cockpit);
        return false;
    }
    if (!ShaderLoadFile(&cockpit->shader, SHADER_COCKPIT_PATH, message, size))
    {
        CockpitUnload(cockpit);
        return false;
    }

    const CockpitMetrics *metrics = &cockpit->metrics;
    if (cockpit->wheel.width != metrics->wheel_frame_width * metrics->wheel_frame_count ||
        cockpit->wheel.height != metrics->wheel_frame_height)
    {
        snprintf(message, size, "%s does not fit the wheel frames in %s", COCKPIT_WHEEL_PATH,
                 COCKPIT_METRICS_PATH);
        CockpitUnload(cockpit);
        return false;
    }

    SetTextureFilter(cockpit->panorama, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(cockpit->wheel, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(cockpit->panorama, TEXTURE_WRAP_CLAMP);
    SetTextureWrap(cockpit->wheel, TEXTURE_WRAP_CLAMP);

    Shader shader = cockpit->shader;
    ShaderSetVec2(shader, "panoSize", (float)cockpit->panorama.width,
                  (float)cockpit->panorama.height);
    ShaderSetFloat(shader, "ppd", metrics->px_per_deg);
    ShaderSetFloat(shader, "yawMin", metrics->yaw_min_deg);
    ShaderSetFloat(shader, "latTop", metrics->lat_top_deg);
    ShaderSetVec4(shader, "wheelRect", (float)metrics->wheel_x, (float)metrics->wheel_y,
                  (float)metrics->wheel_frame_width, (float)metrics->wheel_frame_height);
    ShaderSetVec2(shader, "wheelAtlasSize", (float)cockpit->wheel.width,
                  (float)cockpit->wheel.height);

    cockpit->glass = LoadRenderTexture((int)(cockpit->panorama.width * GLASS_SCALE),
                                       (int)(cockpit->panorama.height * GLASS_SCALE));
    if (!IsRenderTextureValid(cockpit->glass))
    {
        snprintf(message, size, "Could not make the picture of the windshield");
        CockpitUnload(cockpit);
        return false;
    }
    SetTextureFilter(cockpit->glass.texture, TEXTURE_FILTER_POINT);
    BeginTextureMode(cockpit->glass);
    ClearBackground(BLANK);
    EndTextureMode();
    return true;
}

void CockpitRenderGlass(Cockpit *cockpit, const VomitBlob *blobs, int count)
{
    bool any = false;
    for (int i = 0; i < count; i++)
    {
        any = any || blobs[i].alive;
    }
    if (!any && !cockpit->glass_used)
    {
        return;
    }
    // A place on the glass in degrees becomes a place in the panorama, then the glass picture is half that size
    const CockpitMetrics *metrics = &cockpit->metrics;
    float ppd = metrics->px_per_deg;
    BeginTextureMode(cockpit->glass);
    ClearBackground(BLANK);
    BeginMode2D((Camera2D){.zoom = GLASS_SCALE});
    for (int i = 0; i < count; i++)
    {
        const VomitBlob *blob = &blobs[i];
        if (!blob->alive)
        {
            continue;
        }
        Vector2 center = {(blob->position.x - metrics->yaw_min_deg) * ppd,
                          (metrics->lat_top_deg + blob->position.y) * ppd};
        VomitDraw(center, blob->radius * ppd, blob->seed, blob->radius > VOMIT_BIG, 1.0f);
    }
    EndMode2D();
    EndTextureMode();
    cockpit->glass_used = any;
}

void CockpitUnload(Cockpit *cockpit)
{
    if (IsRenderTextureValid(cockpit->glass))
    {
        UnloadRenderTexture(cockpit->glass);
    }
    if (IsShaderValid(cockpit->shader))
    {
        UnloadShader(cockpit->shader);
    }
    if (IsTextureValid(cockpit->wheel))
    {
        UnloadTexture(cockpit->wheel);
    }
    if (IsTextureValid(cockpit->panorama))
    {
        UnloadTexture(cockpit->panorama);
    }
    *cockpit = (Cockpit){0};
}

// Return the index of the wheel frame with the angle nearest to an angle
static int NearestWheelFrame(const CockpitMetrics *metrics, float angle_deg)
{
    int best = 0;
    for (int i = 1; i < metrics->wheel_frame_count; i++)
    {
        if (fabsf(metrics->wheel_angles_deg[i] - angle_deg) <
            fabsf(metrics->wheel_angles_deg[best] - angle_deg))
        {
            best = i;
        }
    }
    return best;
}

void CockpitPitchLimits(const Cockpit *cockpit, float *pitch_min, float *pitch_max)
{
    float half_height_deg =
        atanf(tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD) * VIEW_HEIGHT / VIEW_WIDTH) * RAD2DEG;
    *pitch_min = half_height_deg - cockpit->metrics.lat_top_deg;
    *pitch_max = -cockpit->metrics.lat_bottom_deg - half_height_deg;
}

void CockpitDraw(const Cockpit *cockpit, Rectangle destination, float head_yaw, float head_pitch,
                 float steer, Vector3 tint)
{
    const CockpitMetrics *metrics = &cockpit->metrics;
    Shader shader = cockpit->shader;

    // The focal length follows the size of the area, so the field of view stays the same
    float focal = (destination.width / 2.0f) / tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD);
    float full_lock_deg = metrics->wheel_angles_deg[metrics->wheel_frame_count - 1];
    int frame = NearestWheelFrame(metrics, steer * full_lock_deg);

    ShaderSetFloat(shader, "focal", focal);
    ShaderSetVec2(shader, "center", destination.x + destination.width / 2.0f,
                  (float)GetScreenHeight() - (destination.y + destination.height / 2.0f));
    ShaderSetFloat(shader, "headYaw", head_yaw);
    ShaderSetFloat(shader, "pitchDown", head_pitch);
    ShaderSetFloat(shader, "wheelFrame", (float)frame);
    SetShaderValue(shader, GetShaderLocation(shader, "tint"), &tint, SHADER_UNIFORM_VEC3);

    BeginShaderMode(shader);
    ShaderSetTexture(shader, "panorama", cockpit->panorama);
    ShaderSetTexture(shader, "wheelTex", cockpit->wheel);
    ShaderSetTexture(shader, "glassTex", cockpit->glass.texture);
    DrawRectangle((int)destination.x, (int)destination.y, (int)destination.width,
                  (int)destination.height, WHITE);
    EndShaderMode();
}
