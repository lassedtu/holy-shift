#include "render/shader_util.h"

#include <stdio.h>

#include "rlgl.h"

bool ShaderLoadFile(Shader *shader, const char *path, char *message, size_t size)
{
    *shader = (Shader){0};
    if (!FileExists(path))
    {
        snprintf(message, size, "Could not find the shader %s", path);
        return false;
    }

    Shader loaded = LoadShader(NULL, path);
    if (!IsShaderValid(loaded) || loaded.id == rlGetShaderIdDefault())
    {
        snprintf(message, size, "Could not compile the shader %s, see the log for the errors", path);
        return false;
    }
    *shader = loaded;
    return true;
}

void ShaderSetFloat(Shader shader, const char *name, float value)
{
    SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_FLOAT);
}

void ShaderSetVec2(Shader shader, const char *name, float x, float y)
{
    float value[2] = {x, y};
    SetShaderValue(shader, GetShaderLocation(shader, name), value, SHADER_UNIFORM_VEC2);
}

void ShaderSetVec4(Shader shader, const char *name, float x, float y, float z, float w)
{
    float value[4] = {x, y, z, w};
    SetShaderValue(shader, GetShaderLocation(shader, name), value, SHADER_UNIFORM_VEC4);
}

void ShaderSetColor(Shader shader, const char *name, Color color)
{
    float value[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    SetShaderValue(shader, GetShaderLocation(shader, name), value, SHADER_UNIFORM_VEC3);
}

void ShaderSetTexture(Shader shader, const char *name, Texture2D texture)
{
    SetShaderValueTexture(shader, GetShaderLocation(shader, name), texture);
}
