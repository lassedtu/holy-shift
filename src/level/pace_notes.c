#include "level/pace_notes.h"

#include <math.h>
#include <stdio.h>

#include "core/config.h"

#define SCAN_STEP 4.0f      // distance between two examined positions, in map pixels
#define RADIUS_WINDOW 16.0f // the radius at a position is measured over this distance
#define MIN_TURN (15.0f * DEG2RAD) // a bend that turns less than this is not a corner
#define MERGE_VALLEY 0.6f   // two peaks are one corner when the bend between them stays above this part of the smaller peak
#define HAIRPIN_TURN (135.0f * DEG2RAD)
#define ENTRY_PART 0.5f // the entry of a corner is where the bend gets to this part of the peak

// The largest radius of each grade, in meters
static const float GRADE_RADIUS_M[] = {12.0f, 20.0f, 32.0f, 50.0f, 75.0f, 110.0f};
#define GRADE_COUNT ((int)(sizeof(GRADE_RADIUS_M) / sizeof(GRADE_RADIUS_M[0])))

int PaceGrade(float radius)
{
    float meters = fabsf(radius) / MAP_PIXELS_PER_METER;
    for (int i = 0; i < GRADE_COUNT; i++)
    {
        if (meters < GRADE_RADIUS_M[i])
        {
            return i + 1;
        }
    }
    return PACE_GRADE_STRAIGHT;
}

float PaceRadiusAt(const Track *track, float position)
{
    float turn = TrackSignedTurn(track, position - RADIUS_WINDOW / 2.0f, RADIUS_WINDOW);
    if (fabsf(turn) < 1e-5f)
    {
        return INFINITY;
    }
    return RADIUS_WINDOW / turn;
}

// Return the distance from a to b along the track, a closed track goes over the start
static float Ahead(const Track *track, float a, float b)
{
    float d = b - a;
    if (track->closed && d < 0.0f)
    {
        d += track->length;
    }
    return d;
}

// Return the sample index i + offset, a closed track goes over the start, -1 when it is outside an open track
static int Step(int i, int offset, int count, bool closed)
{
    i += offset;
    if (closed)
    {
        return (i % count + count) % count;
    }
    return i < 0 || i >= count ? -1 : i;
}

void PaceNotesBuild(PaceNotes *notes, const Track *track)
{
    notes->count = 0;
    int count = (int)(track->length / SCAN_STEP);
    if (count < 3)
    {
        return;
    }
    float *bend = MemAlloc((unsigned int)count * sizeof(float)); // 1 / radius, positive to the right
    for (int i = 0; i < count; i++)
    {
        float radius = PaceRadiusAt(track, (float)i * SCAN_STEP);
        bend[i] = isinf(radius) ? 0.0f : 1.0f / radius;
    }

    float min_bend = 1.0f / (GRADE_RADIUS_M[GRADE_COUNT - 1] * MAP_PIXELS_PER_METER);
    bool closed = track->closed;
    for (int i = 0; i < count && notes->count < PACE_MAX_CORNERS; i++)
    {
        // A corner is around each peak of the bend, and goes on to the lowest bend on each side
        float peak = fabsf(bend[i]);
        int before = Step(i, -1, count, closed);
        int after = Step(i, 1, count, closed);
        bool is_peak = peak >= min_bend && (before < 0 || fabsf(bend[before]) < peak) &&
                       (after < 0 || fabsf(bend[after]) <= peak);
        if (!is_peak)
        {
            continue;
        }
        float sign = bend[i] > 0.0f ? 1.0f : -1.0f;
        int start = i;
        int end = i;
        float turn = peak * SCAN_STEP;
        for (int n = Step(start, -1, count, closed); n >= 0 && n != i; n = Step(start, -1, count, closed))
        {
            float b = bend[n] * sign;
            if (b < min_bend || b > bend[start] * sign)
            {
                break;
            }
            start = n;
            turn += b * SCAN_STEP;
        }
        for (int n = Step(end, 1, count, closed); n >= 0 && n != start; n = Step(end, 1, count, closed))
        {
            float b = bend[n] * sign;
            if (b < min_bend || b > bend[end] * sign)
            {
                break;
            }
            end = n;
            turn += b * SCAN_STEP;
        }

        int entry = start;
        while (entry != i && fabsf(bend[entry]) < ENTRY_PART * peak)
        {
            entry = Step(entry, 1, count, closed);
        }

        PaceCorner corner = {
            .start = (float)start * SCAN_STEP,
            .entry = (float)entry * SCAN_STEP,
            .apex = (float)i * SCAN_STEP,
            .end = (float)end * SCAN_STEP,
            .direction = (int)sign,
            .radius = 1.0f / peak,
            .turn = turn,
        };

        // A low valley between two peaks in the same direction does not make two corners
        PaceCorner *last = notes->count > 0 ? &notes->corners[notes->count - 1] : NULL;
        float valley = fabsf(bend[start]);
        if (last != NULL && last->direction == corner.direction &&
            Ahead(track, last->end, corner.start) <= SCAN_STEP &&
            valley >= MERGE_VALLEY / fmaxf(last->radius, corner.radius))
        {
            last->end = corner.end;
            last->turn += corner.turn;
            if (corner.radius < last->radius)
            {
                last->radius = corner.radius;
                last->apex = corner.apex;
            }
            continue;
        }
        notes->corners[notes->count++] = corner;
    }
    MemFree(bend);

    // Remove the bends that turn too little to be a corner
    int kept = 0;
    for (int i = 0; i < notes->count; i++)
    {
        PaceCorner corner = notes->corners[i];
        if (corner.turn < MIN_TURN)
        {
            continue;
        }
        corner.grade = PaceGrade(corner.radius);
        corner.hairpin = corner.grade == 1 && corner.turn >= HAIRPIN_TURN;
        notes->corners[kept++] = corner;
    }
    notes->count = kept;
}

int PaceNextCorner(const PaceNotes *notes, const Track *track, float position, float *distance)
{
    position = TrackWrap(track, position);
    int best = -1;
    float best_distance = INFINITY;
    for (int i = 0; i < notes->count; i++)
    {
        float d = notes->corners[i].entry - position;
        if (track->closed && d < 0.0f)
        {
            d += track->length;
        }
        if (d >= 0.0f && d < best_distance)
        {
            best = i;
            best_distance = d;
        }
    }
    *distance = best_distance;
    return best;
}

// The words of each grade, from grade 1 to grade 6
static const char *const GRADE_WORDS[] = {"VERY SHARP", "SHARP", "MEDIUM", "EASY", "FAST", "SLIGHT"};

const char *PaceGradeWord(const PaceCorner *corner)
{
    return corner->hairpin ? "HAIRPIN" : GRADE_WORDS[corner->grade - 1];
}

const char *PaceSideWord(const PaceCorner *corner)
{
    return corner->direction > 0 ? "RIGHT" : "LEFT";
}

void PaceCall(const PaceCorner *corner, char *text, int size)
{
    snprintf(text, (size_t)size, "%s %s", PaceGradeWord(corner), PaceSideWord(corner));
}

void PaceShortCall(const PaceCorner *corner, char *text, int size)
{
    char side = corner->direction > 0 ? 'R' : 'L';
    if (corner->hairpin)
    {
        snprintf(text, (size_t)size, "HP%c", side);
    }
    else
    {
        snprintf(text, (size_t)size, "%d%c", corner->grade, side);
    }
}
