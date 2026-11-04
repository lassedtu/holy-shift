#include "render/mode7.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "core/config.h"
#include "render/shader_util.h"

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

    // The horizon is where the eye level is in the tilted view of the cabin
    float focal = (VIEW_WIDTH / 2.0f) / tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD);
    float horizon = VIEW_HEIGHT / 2.0f - focal * tanf(VIEW_PITCH_DEG * DEG2RAD);

    Shader shader = mode7->shader;
    ShaderSetVec2(shader, "resolution", VIEW_WIDTH, VIEW_HEIGHT);
    ShaderSetFloat(shader, "camHeight", CAMERA_HEIGHT);
    ShaderSetFloat(shader, "focal", focal);
    ShaderSetFloat(shader, "horizon", horizon);
    ShaderSetColor(shader, "skyColor", COLOR_SKY);
    ShaderSetColor(shader, "outsideColor", COLOR_OUTSIDE);
    ShaderSetVec2(shader, "fog", FOG_START, FOG_END);
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

void Mode7Render(Mode7 *mode7, Texture2D ground, Vector2 position, float heading)
{
    Shader shader = mode7->shader;
    ShaderSetVec2(shader, "mapSize", (float)ground.width, (float)ground.height);
    ShaderSetVec2(shader, "camPos", position.x, position.y);
    ShaderSetVec2(shader, "camDir", cosf(heading), sinf(heading));

    BeginTextureMode(mode7->target);
    ClearBackground(COLOR_SKY);
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
