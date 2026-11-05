#ifndef LEVEL_TREE_SPECIES_H
#define LEVEL_TREE_SPECIES_H

// The kinds of trees a level can have
// Each species has some variants of the same tree, and one of them is picked at random for each tree
// The pictures of a variant are in assets/trees/<species>_v<number>.png
// The level editor uses this file too, so it must not use any other file of the game

#define TREE_SPECIES_COUNT 4
#define TREE_VARIANTS_PER_SPECIES 2
#define TREE_MAX_VARIANTS (TREE_SPECIES_COUNT * TREE_VARIANTS_PER_SPECIES)
#define TREE_VARIANT_NAME_MAX 24

static const char *const TREE_SPECIES_NAMES[TREE_SPECIES_COUNT] = {"spruce", "maple", "birch", "oak"};

// A level without a list of trees has the first species
#define TREE_SPECIES_DEFAULT 0

#endif
