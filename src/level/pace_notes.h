#ifndef LEVEL_PACE_NOTES_H
#define LEVEL_PACE_NOTES_H

#include <stdbool.h>

#include "level/track.h"

#define PACE_MAX_CORNERS 256
#define PACE_GRADE_STRAIGHT 7 // a part of the track that is not a corner

// One corner of the track, found from the curve
// The positions are distances from the start of the track, in map pixels
// An end before the start is possible on a closed track, then the corner goes over the start
typedef struct PaceCorner
{
    float start;
    float entry; // the position where the road starts to turn hard, the distance to the corner is measured to it
    float apex;  // the position of the smallest radius
    float end;
    int direction; // 1 is right, -1 is left
    int grade;     // 1 (slowest) to 6 (fastest)
    float radius;  // smallest radius, in map pixels
    float turn;    // total change of direction, in radians
    bool hairpin;
} PaceCorner;

// The pace notes of a track: all the corners in the order of the track
typedef struct PaceNotes
{
    PaceCorner corners[PACE_MAX_CORNERS];
    int count;
} PaceNotes;

// Find the corners of a track
void PaceNotesBuild(PaceNotes *notes, const Track *track);

// Return the grade of a radius in map pixels, PACE_GRADE_STRAIGHT when the radius is too large for a corner
int PaceGrade(float radius);

// Return the signed radius of the track at a position, in map pixels
// A positive radius turns right. The radius is INFINITY on a straight part
float PaceRadiusAt(const Track *track, float position);

// Find the next corner with its entry after a position
// Return its index, or -1 when there is no corner after the position
// The distance from the position to the entry of the corner goes in distance
int PaceNextCorner(const PaceNotes *notes, const Track *track, float position, float *distance);

// Return the word of the grade of a corner, for example "SHARP" or "HAIRPIN"
const char *PaceGradeWord(const PaceCorner *corner);

// Return "LEFT" or "RIGHT"
const char *PaceSideWord(const PaceCorner *corner);

// Write the call of a corner, for example "SHARP RIGHT", "MEDIUM LEFT" or "HAIRPIN RIGHT"
void PaceCall(const PaceCorner *corner, char *text, int size);

// Write the short mark of a corner for the map, for example "3L", "6R" or "HPL" for a hairpin
void PaceShortCall(const PaceCorner *corner, char *text, int size);

#endif
