#ifndef RENDER_PACE_MAP_H
#define RENDER_PACE_MAP_H

#include <stdbool.h>

#include "car/car.h"
#include "level/pace_notes.h"
#include "level/track.h"
#include "raylib.h"

// The map that Luka holds at the right side of the cabin
// The map shows the road ahead with the pace notes, the top of the map points along the road
// The text at the bottom gives the next corner
typedef struct PaceMap
{
    RenderTexture2D paper; // the drawn map, flat
    RenderTexture2D view;  // the map as seen from the head of the driver, the size of the game view
    float position;        // position of the car on the track
    float heading;         // direction of the top of the map, in radians
    float time;
    bool visible; // false when the head looks away from the map, then nothing is rendered
} PaceMap;

bool PaceMapInit(PaceMap *map);
void PaceMapUnload(PaceMap *map);

// Put the map at the car again, for example when the stage starts again
void PaceMapReset(PaceMap *map, const Track *track, const Car *car);

void PaceMapUpdate(PaceMap *map, const Track *track, const Car *car, float dt);

// Draw the map into its textures, before BeginDrawing
// Nothing is drawn when the map is out of the view
// The head yaw and the head pitch are in degrees, a positive pitch looks down
void PaceMapRender(PaceMap *map, const Track *track, const PaceNotes *notes, const Car *car,
                   float head_yaw, float head_pitch);

// Draw the last rendered view over an area of the window
void PaceMapDraw(const PaceMap *map, Rectangle destination);

#endif
