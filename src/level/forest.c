#include "level/forest.h"

#include <math.h>
#include <stdlib.h>

#include "core/config.h"

#define ROAD_CELL_SIZE 32 // cell size of the grid of road samples
#define SPACING_CELL_SIZE ((int)FOREST_MIN_SPACING)

// Small random number generator, so a level always gets the same forest
static unsigned int Random(unsigned int *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

// Return a number from 0 to 1
static float RandomUnit(unsigned int *state)
{
    return (float)(Random(state) & 0xFFFF) / 65535.0f;
}

// Cells that hold indices, in the same way as the compact lists of the forest
typedef struct IndexGrid
{
    int *start;
    int *items;
    int columns;
    int rows;
    int cell_size;
} IndexGrid;

static int CellOf(const IndexGrid *grid, Vector2 point, int *column, int *row)
{
    *column = (int)floorf(point.x / (float)grid->cell_size);
    *row = (int)floorf(point.y / (float)grid->cell_size);
    return *column >= 0 && *column < grid->columns && *row >= 0 && *row < grid->rows;
}

static void IndexGridFree(IndexGrid *grid)
{
    free(grid->start);
    free(grid->items);
    *grid = (IndexGrid){0};
}

// Put the track samples in a grid, to find the road near a point
static bool RoadGridBuild(IndexGrid *grid, const Track *track, const Level *level)
{
    *grid = (IndexGrid){0};
    grid->cell_size = ROAD_CELL_SIZE;
    grid->columns = level->map_width / ROAD_CELL_SIZE + 1;
    grid->rows = level->map_height / ROAD_CELL_SIZE + 1;
    int cells = grid->columns * grid->rows;
    grid->start = calloc((size_t)cells + 1, sizeof(int));
    grid->items = malloc((size_t)track->count * sizeof(int));
    int *fill = calloc((size_t)cells, sizeof(int));
    if (grid->start == NULL || grid->items == NULL || fill == NULL)
    {
        free(fill);
        IndexGridFree(grid);
        return false;
    }

    int column;
    int row;
    for (int i = 0; i < track->count; i++)
    {
        if (CellOf(grid, track->samples[i], &column, &row))
        {
            grid->start[row * grid->columns + column + 1]++;
        }
    }
    for (int c = 0; c < cells; c++)
    {
        grid->start[c + 1] += grid->start[c];
    }
    for (int i = 0; i < track->count; i++)
    {
        if (CellOf(grid, track->samples[i], &column, &row))
        {
            int cell = row * grid->columns + column;
            grid->items[grid->start[cell] + fill[cell]++] = i;
        }
    }
    free(fill);
    return true;
}

// True when a point is nearer to the middle of the road than the distance
static bool NearRoad(const IndexGrid *grid, const Track *track, Vector2 point, float distance)
{
    int column;
    int row;
    int reach = (int)ceilf(distance / (float)grid->cell_size);
    CellOf(grid, point, &column, &row);
    for (int r = row - reach; r <= row + reach; r++)
    {
        for (int c = column - reach; c <= column + reach; c++)
        {
            if (c < 0 || c >= grid->columns || r < 0 || r >= grid->rows)
            {
                continue;
            }
            int cell = r * grid->columns + c;
            for (int k = grid->start[cell]; k < grid->start[cell + 1]; k++)
            {
                Vector2 sample = track->samples[grid->items[k]];
                float dx = sample.x - point.x;
                float dy = sample.y - point.y;
                if (dx * dx + dy * dy < distance * distance)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// Grid of the trees that are placed, as linked lists, to keep the trunks apart
typedef struct SpacingGrid
{
    int *head;
    int *next;
    int columns;
    int rows;
    Vector2 origin;
} SpacingGrid;

static bool NearTree(const SpacingGrid *grid, const Tree *trees, Vector2 point)
{
    int column = (int)floorf((point.x - grid->origin.x) / SPACING_CELL_SIZE);
    int row = (int)floorf((point.y - grid->origin.y) / SPACING_CELL_SIZE);
    for (int r = row - 1; r <= row + 1; r++)
    {
        for (int c = column - 1; c <= column + 1; c++)
        {
            if (c < 0 || c >= grid->columns || r < 0 || r >= grid->rows)
            {
                continue;
            }
            for (int i = grid->head[r * grid->columns + c]; i >= 0; i = grid->next[i])
            {
                float dx = trees[i].position.x - point.x;
                float dy = trees[i].position.y - point.y;
                if (dx * dx + dy * dy < FOREST_MIN_SPACING * FOREST_MIN_SPACING)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

static int ForestCellIndex(const Forest *forest, Vector2 position)
{
    int column = (int)((position.x - forest->origin.x) / FOREST_CELL_SIZE);
    int row = (int)((position.y - forest->origin.y) / FOREST_CELL_SIZE);
    return row * forest->columns + column;
}

// Sort the trees by cell into the final arrays of the forest
static bool ForestStore(Forest *forest, const Tree *trees, int count)
{
    int cells = forest->columns * forest->rows;
    forest->cell_start = calloc((size_t)cells + 1, sizeof(int));
    forest->trees = malloc((size_t)(count > 0 ? count : 1) * sizeof(Tree));
    int *fill = calloc((size_t)cells, sizeof(int));
    if (forest->cell_start == NULL || forest->trees == NULL || fill == NULL)
    {
        free(fill);
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        forest->cell_start[ForestCellIndex(forest, trees[i].position) + 1]++;
    }
    for (int c = 0; c < cells; c++)
    {
        forest->cell_start[c + 1] += forest->cell_start[c];
    }
    for (int i = 0; i < count; i++)
    {
        int cell = ForestCellIndex(forest, trees[i].position);
        forest->trees[forest->cell_start[cell] + fill[cell]++] = trees[i];
    }
    forest->count = count;
    free(fill);
    return true;
}

bool ForestBuild(Forest *forest, const Level *level, const Track *track)
{
    *forest = (Forest){0};

    // The forest goes past the edge of the map, because the camera can look out of the map
    float width = (float)level->map_width + 2.0f * FOREST_BORDER;
    float height = (float)level->map_height + 2.0f * FOREST_BORDER;
    forest->origin = (Vector2){-FOREST_BORDER, -FOREST_BORDER};
    forest->columns = (int)ceilf(width / FOREST_CELL_SIZE);
    forest->rows = (int)ceilf(height / FOREST_CELL_SIZE);

    IndexGrid road;
    SpacingGrid spacing = {0};
    spacing.origin = forest->origin;
    spacing.columns = (int)ceilf(width / SPACING_CELL_SIZE);
    spacing.rows = (int)ceilf(height / SPACING_CELL_SIZE);
    spacing.head = malloc((size_t)spacing.columns * (size_t)spacing.rows * sizeof(int));
    spacing.next = malloc(FOREST_MAX_TREES * sizeof(int));
    Tree *placed = malloc(FOREST_MAX_TREES * sizeof(Tree));
    bool road_ready = RoadGridBuild(&road, track, level);
    if (spacing.head == NULL || spacing.next == NULL || placed == NULL || !road_ready)
    {
        free(spacing.head);
        free(spacing.next);
        free(placed);
        if (road_ready)
        {
            IndexGridFree(&road);
        }
        return false;
    }
    for (int i = 0; i < spacing.columns * spacing.rows; i++)
    {
        spacing.head[i] = -1;
    }

    char variant_names[TREE_MAX_VARIANTS][TREE_VARIANT_NAME_MAX];
    int variant_count = LevelTreeVariants(level, variant_names);

    // One tree in each square of a grid, in a random place of the square
    unsigned int state = FOREST_SEED;
    float road_clearance = (float)level->road_width * 0.5f + FOREST_ROAD_MARGIN;
    int count = 0;
    int steps_x = (int)ceilf(width / FOREST_SPACING);
    int steps_y = (int)ceilf(height / FOREST_SPACING);
    for (int gy = 0; gy < steps_y; gy++)
    {
        for (int gx = 0; gx < steps_x; gx++)
        {
            Vector2 point = {forest->origin.x + ((float)gx + RandomUnit(&state)) * FOREST_SPACING,
                             forest->origin.y + ((float)gy + RandomUnit(&state)) * FOREST_SPACING};
            int variant = variant_count > 0 ? (int)(Random(&state) % (unsigned int)variant_count) : 0;
            float scale = TREE_SCALE_MIN + RandomUnit(&state) * (TREE_SCALE_MAX - TREE_SCALE_MIN);

            bool inside = point.x < forest->origin.x + width && point.y < forest->origin.y + height;
            if (variant_count == 0 || !inside || count >= FOREST_MAX_TREES ||
                NearRoad(&road, track, point, road_clearance) ||
                NearTree(&spacing, placed, point))
            {
                continue;
            }
            placed[count] = (Tree){variant, point, scale};
            int cell = (int)floorf((point.y - spacing.origin.y) / SPACING_CELL_SIZE) * spacing.columns +
                       (int)floorf((point.x - spacing.origin.x) / SPACING_CELL_SIZE);
            spacing.next[count] = spacing.head[cell];
            spacing.head[cell] = count;
            count++;
        }
    }

    bool stored = ForestStore(forest, placed, count);
    free(spacing.head);
    free(spacing.next);
    free(placed);
    IndexGridFree(&road);
    if (!stored)
    {
        ForestUnload(forest);
    }
    return stored;
}

void ForestUnload(Forest *forest)
{
    free(forest->trees);
    free(forest->cell_start);
    *forest = (Forest){0};
}

const Tree *ForestCell(const Forest *forest, int column, int row, int *count)
{
    if (column < 0 || column >= forest->columns || row < 0 || row >= forest->rows)
    {
        *count = 0;
        return NULL;
    }
    int cell = row * forest->columns + column;
    *count = forest->cell_start[cell + 1] - forest->cell_start[cell];
    return forest->trees + forest->cell_start[cell];
}

bool ForestHit(const Forest *forest, Vector2 center, float heading, float length, float width)
{
    Vector2 forward = {cosf(heading), sinf(heading)};
    float half_length = length / 2.0f;
    float half_width = width / 2.0f;
    float reach = sqrtf(half_length * half_length + half_width * half_width) + TREE_TRUNK_RADIUS * TREE_SCALE_MAX;
    int first_column = (int)floorf((center.x - reach - forest->origin.x) / FOREST_CELL_SIZE);
    int last_column = (int)floorf((center.x + reach - forest->origin.x) / FOREST_CELL_SIZE);
    int first_row = (int)floorf((center.y - reach - forest->origin.y) / FOREST_CELL_SIZE);
    int last_row = (int)floorf((center.y + reach - forest->origin.y) / FOREST_CELL_SIZE);
    for (int row = first_row; row <= last_row; row++)
    {
        for (int column = first_column; column <= last_column; column++)
        {
            int count;
            const Tree *cell = ForestCell(forest, column, row, &count);
            for (int i = 0; i < count; i++)
            {
                // The trunk in the frame of the box, then the nearest point of the box to the trunk
                float dx = cell[i].position.x - center.x;
                float dy = cell[i].position.y - center.y;
                float along = dx * forward.x + dy * forward.y;
                float across = dx * -forward.y + dy * forward.x;
                float out_along = fmaxf(fabsf(along) - half_length, 0.0f);
                float out_across = fmaxf(fabsf(across) - half_width, 0.0f);
                float radius = TREE_TRUNK_RADIUS * cell[i].scale;
                if (out_along * out_along + out_across * out_across < radius * radius)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
