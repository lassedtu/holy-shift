#include "level/track.h"

#include <math.h>
#include <stdlib.h>

#include "raymath.h"
#include "util/math_util.h"

#define CURVE_STEP 2.0f // maximum distance between two curve samples

// Return the number of curve samples between two points
static int SegmentSteps(Vector2 a, Vector2 b)
{
    int steps = (int)ceilf(Vector2Distance(a, b) / CURVE_STEP);
    return steps < 1 ? 1 : steps;
}

bool TrackBuild(Track *track, const Level *level)
{
    *track = (Track){0};
    int n = level->count;
    int segments = level->closed ? n : n - 1;
    int total = 1;
    for (int i = 0; i < segments; i++)
    {
        total += SegmentSteps(level->points[i], level->points[(i + 1) % n]);
    }

    track->samples = malloc((size_t)total * sizeof(Vector2));
    track->distance = malloc((size_t)total * sizeof(float));
    if (track->samples == NULL || track->distance == NULL)
    {
        TrackUnload(track);
        return false;
    }

    int k = 0;
    for (int i = 0; i < segments; i++)
    {
        int before;
        int after;
        if (level->closed)
        {
            before = (i + n - 1) % n;
            after = (i + 2) % n;
        }
        else
        {
            before = i > 0 ? i - 1 : 0;
            after = i + 2 < n ? i + 2 : n - 1;
        }
        Vector2 start = level->points[i];
        Vector2 end = level->points[(i + 1) % n];
        int steps = SegmentSteps(start, end);
        for (int s = 0; s < steps; s++)
        {
            track->samples[k++] = GetSplinePointCatmullRom(
                level->points[before], start, end, level->points[after], (float)s / (float)steps);
        }
    }
    track->samples[k++] = level->closed ? level->points[0] : level->points[n - 1];
    track->count = k;
    track->closed = level->closed;

    track->distance[0] = 0.0f;
    for (int i = 1; i < k; i++)
    {
        track->distance[i] =
            track->distance[i - 1] + Vector2Distance(track->samples[i - 1], track->samples[i]);
    }
    track->length = track->distance[k - 1];
    if (track->length <= 1.0f)
    {
        TrackUnload(track);
        return false;
    }
    return true;
}

void TrackUnload(Track *track)
{
    free(track->samples);
    free(track->distance);
    *track = (Track){0};
}

float TrackWrap(const Track *track, float position)
{
    if (track->closed)
    {
        position = fmodf(position, track->length);
        return position < 0.0f ? position + track->length : position;
    }
    return Clamp(position, 0.0f, track->length);
}

Vector2 TrackPoint(const Track *track, float position)
{
    position = TrackWrap(track, position);

    int low = 0;
    int high = track->count - 1;
    while (high - low > 1)
    {
        int middle = (low + high) / 2;
        if (track->distance[middle] <= position)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    float span = track->distance[high] - track->distance[low];
    float t = span > 0.0f ? (position - track->distance[low]) / span : 0.0f;
    return Vector2Lerp(track->samples[low], track->samples[high], t);
}

float TrackHeading(const Track *track, float position)
{
    Vector2 here = TrackPoint(track, position);
    Vector2 ahead = TrackPoint(track, position + 5.0f);
    return atan2f(ahead.y - here.y, ahead.x - here.x);
}

float TrackNearest(const Track *track, Vector2 point, float around, float window)
{
    float best = around;
    float best_distance = INFINITY;
    for (float offset = -window; offset <= window; offset += CURVE_STEP)
    {
        float distance = Vector2DistanceSqr(TrackPoint(track, around + offset), point);
        if (distance < best_distance)
        {
            best_distance = distance;
            best = around + offset;
        }
    }
    return TrackWrap(track, best);
}

float TrackTurnAhead(const Track *track, float position, float distance, float step)
{
    float total = 0.0f;
    float previous_heading = TrackHeading(track, position);
    for (float d = step; d <= distance; d += step)
    {
        float heading = TrackHeading(track, position + d);
        total += fabsf(AngleDifference(previous_heading, heading));
        previous_heading = heading;
    }
    return total;
}

float TrackSignedTurn(const Track *track, float position, float distance)
{
    return AngleDifference(TrackHeading(track, position), TrackHeading(track, position + distance));
}
