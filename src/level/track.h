#ifndef LEVEL_TRACK_H
#define LEVEL_TRACK_H

#include <stdbool.h>

#include "level/level.h"
#include "raylib.h"

// The road curve of a level, as many short steps with the distance from the start
// A position on the track is a distance from the start, in map pixels
typedef struct Track
{
    Vector2 *samples;
    float *distance; // distance from the start to each sample
    int count;
    float length;
    bool closed;
} Track;

// Make the curve through the points of the level, the same curve as the level editor makes
bool TrackBuild(Track *track, const Level *level);
void TrackUnload(Track *track);

// Keep a position on the track: a closed track repeats, an open track stops at its ends
float TrackWrap(const Track *track, float position);

// Return the point and the heading (radians) of the track at a position
Vector2 TrackPoint(const Track *track, float position);
float TrackHeading(const Track *track, float position);

// Return the total turn (radians, always positive) of the track in a distance from a position
float TrackTurnAhead(const Track *track, float position, float distance, float step);

// Return the heading change (radians) between a position and a distance after it
// A turn to the right is positive because the map y axis points down
float TrackSignedTurn(const Track *track, float position, float distance);

#endif
