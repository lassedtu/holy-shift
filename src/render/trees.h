#ifndef RENDER_TREES_H
#define RENDER_TREES_H

#include <stdbool.h>
#include <stddef.h>

#include "level/forest.h"
#include "level/level.h"
#include "raylib.h"
#include "render/mode7.h"
#include "render/tree_sheet.h"

#define TREE_LOD_COUNT 3

// One tree that is in the view, ready to draw
typedef struct VisibleTree
{
    float depth; // distance in front of the camera
    float side;  // distance to the right of the camera
    const Tree *tree;
} VisibleTree;

// The pictures of one variant of a tree
// The pictures get smaller and have fewer directions for the trees that are far away
typedef struct TreeVariant
{
    TreeSheet sheets[TREE_LOD_COUNT]; // from the most to the least detailed
} TreeVariant;

// The trees in the road view
typedef struct Trees
{
    Shader shader;
    Shader shadow_shader; // draws the shadows as dots
    TreeVariant variants[TREE_MAX_VARIANTS]; // in the order of LevelTreeVariants
    int variant_count;
    Texture2D shadow; // black circle that fades out at the edge
    VisibleTree *visible;
} Trees;

// Load the shader and the pictures of the trees of a level, write the reason in message when it fails
bool TreesInit(Trees *trees, const Level *level, char *message, size_t size);
void TreesUnload(Trees *trees);

// Draw the trees on top of the road view
// The target is the view of Mode7Render, the camera is the same as for that call
void TreesRender(Trees *trees, const Forest *forest, RenderTexture2D target, Mode7Camera camera);

#endif
