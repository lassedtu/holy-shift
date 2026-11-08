#ifndef LEVEL_LOGIC_MAP_H
#define LEVEL_LOGIC_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "level/level.h"
#include "level/track.h"
#include "raylib.h"

// Load the logic map of a level (LEVEL_LOGIC_FILE) as RGBA, write the reason in message when it fails
// With repaint, the road of the file is removed and painted again along the track with the road width of the level,
// as the level builder paints it. Use it when the difficulty changes the road width
bool LogicMapLoad(Image *image, const Level *level, const Track *track, bool repaint, char *message, size_t size);

#endif
