#include "render/mode7.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "core/config.h"
#include "render/shader_util.h"

float Mode7Focal(void)
{
    return (VIEW_WIDTH / 2.0f) / tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD);
}

float Mode7Project(float pitch, float side, float height, float depth, Vector2 *screen)
{
    float angle = pitch * DEG2RAD;
    float up = height * cosf(angle) + depth * sinf(angle);
    float front = depth * cosf(angle) - height * sinf(angle);
    if (front > 0.0f)
    {
        float focal = Mode7Focal();
        screen->x = VIEW_WIDTH / 2.0f + side / front * focal;
        screen->y = VIEW_HEIGHT / 2.0f - up / front * focal;
    }
    return front;
}

bool Mode7Init(Mode7 *mode7, char *message, size_t size)
{
    *mode7 = (Mode7){0};

    if (!ShaderLoadFile(&mode7->shader, SHADER_MODE7_PATH, message, size))
    {
        return false;
    }
    mode7->target = LoadRenderTexture(VIEW_WIDTH, VIEW_HEIGHT);
    if (!IsRenderTextureValid(mode7->target))
    {
        snprintf(message, size, "Could not make the render texture of the road view");
        Mode7Unload(mode7);
        return false;
    }
    SetTextureFilter(mode7->target.texture, TEXTURE_FILTER_POINT);

    float focal = Mode7Focal();

    Shader shader = mode7->shader;
    ShaderSetVec2(shader, "resolution", VIEW_WIDTH, VIEW_HEIGHT);
    ShaderSetFloat(shader, "camHeight", CAMERA_HEIGHT);
    ShaderSetFloat(shader, "focal", focal);
    ShaderSetFloat(shader, "hazeFade", HAZE_FADE);
    ShaderSetColor(shader, "outsideColor", COLOR_OUTSIDE);
    ShaderSetVec2(shader, "headlightReach", HEADLIGHT_NEAR, HEADLIGHT_FAR);
    ShaderSetFloat(shader, "headlightSpread", tanf(HEADLIGHT_HALF_ANGLE_DEG * DEG2RAD));
    ShaderSetVec2(shader, "headlightToe", cosf(HEADLIGHT_TOE_DEG * DEG2RAD), sinf(HEADLIGHT_TOE_DEG * DEG2RAD));
    ShaderSetFloat(shader, "headlightSpacing", HEADLIGHT_SPACING);
    ShaderSetVec2(shader, "headlightCenter", HEADLIGHT_FORWARD, HEADLIGHT_RIGHT);
    return true;
}

void Mode7Unload(Mode7 *mode7)
{
    if (IsRenderTextureValid(mode7->target))
    {
        UnloadRenderTexture(mode7->target);
    }
    if (IsShaderValid(mode7->shader))
    {
        UnloadShader(mode7->shader);
    }
    *mode7 = (Mode7){0};
}

void Mode7Render(Mode7 *mode7, Texture2D ground, Mode7Camera camera, const Lighting *light)
{
    Shader shader = mode7->shader;
    ShaderSetColor(shader, "skyColor", light->sky);
    ShaderSetColor(shader, "fogColor", light->fog);
    ShaderSetVec2(shader, "fog", light->fog_start, light->fog_end);
    // The tops of the tallest trees at the end of the fog are this many rows over the horizon
    float tallest = TREE_HAZE_UNITS * TREE_MAP_PIXELS_PER_UNIT * TREE_SCALE_MAX - CAMERA_HEIGHT;
    ShaderSetFloat(shader, "hazeHeight", tallest * Mode7Focal() / light->fog_end);
    SetShaderValue(shader, GetShaderLocation(shader, "tint"), &light->tint, SHADER_UNIFORM_VEC3);
    ShaderSetFloat(shader, "headlights", light->headlights);
    ShaderSetFloat(shader, "wetness", light->wetness);
    ShaderSetFloat(shader, "time", (float)GetTime());
    ShaderSetColor(shader, "glintColor", light->glint);
    ShaderSetFloat(shader, "glintRate", light->glint.a / 255.0f);
    ShaderSetVec2(shader, "sunDir", cosf(SUN_AZIMUTH_DEG * DEG2RAD), sinf(SUN_AZIMUTH_DEG * DEG2RAD));
    ShaderSetVec2(shader, "carDir", cosf(camera.car_heading), sinf(camera.car_heading));
    ShaderSetVec2(shader, "mapSize", (float)ground.width, (float)ground.height);
    ShaderSetVec2(shader, "camPos", camera.position.x, camera.position.y);
    ShaderSetVec2(shader, "camDir", cosf(camera.heading), sinf(camera.heading));
    ShaderSetFloat(shader, "pitch", camera.pitch * DEG2RAD);

    BeginTextureMode(mode7->target);
    ClearBackground(light->sky);
    BeginShaderMode(shader);
    ShaderSetTexture(shader, "levelTexture", ground);
    DrawRectangle(0, 0, VIEW_WIDTH, VIEW_HEIGHT, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void Mode7Draw(const Mode7 *mode7, Rectangle destination)
{
    // A render texture is upside down, a negative height turns it
    Rectangle source = {0, 0, VIEW_WIDTH, -VIEW_HEIGHT};
    DrawTexturePro(mode7->target.texture, source, destination, (Vector2){0, 0}, 0.0f, WHITE);
}
