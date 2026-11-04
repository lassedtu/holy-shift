#ifndef RENDER_SHADER_UTIL_H
#define RENDER_SHADER_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

// Load a fragment shader from a file, write the reason in message when it fails
// Raylib uses its default shader without a message when a file is missing or a shader does not compile
// This function reports these cases as a failure, and the compiler errors are in the log
bool ShaderLoadFile(Shader *shader, const char *path, char *message, size_t size);

// Set a uniform of a shader by its name
void ShaderSetFloat(Shader shader, const char *name, float value);
void ShaderSetVec2(Shader shader, const char *name, float x, float y);
void ShaderSetVec4(Shader shader, const char *name, float x, float y, float z, float w);
void ShaderSetColor(Shader shader, const char *name, Color color);

// Set a texture uniform of a shader
// Call this after BeginShaderMode, because BeginShaderMode resets the textures
void ShaderSetTexture(Shader shader, const char *name, Texture2D texture);

#endif
