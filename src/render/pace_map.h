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
#define PACE_MAP_PAPER_WIDTH 304 // texture pixels of the paper, about one for each game pixel
#define PACE_MAP_PAPER_HEIGHT 448
#define PACE_MAP_CAR_Y 256.0f    // the car is this far down on the paper
#define PACE_MAP_NOTE_TOP 300.0f // the text about the next corner starts this far down on the paper

// How Luka holds the map now, on top of the normal place in his hands (see game/map_events.h)
typedef struct MapPose
{
    float roll_deg; // turns the map around the line of sight, 180 is upside down
    Vector2 offset; // meters, to the right and up
    float closer;   // from 0 (normal) to 1 (held near the face of Luka, much larger in the view)
    float flip;     // from 0 (front to the player) to 1 (turned around, the blank back to the player)
} MapPose;

typedef struct PaceMap
{
    RenderTexture2D paper; // the drawn map, flat
    RenderTexture2D view;  // the map as seen from the head of the driver, the size of the game view
    float position;        // position of the car on the track
    float heading;         // direction of the top of the map, in radians
    float time;
    bool visible; // false when the head looks away from the map, then nothing is rendered
    Color light;  // the light on the paper, from the time of day
    MapPose pose; // the map events change it each frame
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
