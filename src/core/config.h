#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include "raylib.h"

// Window and view
#define WINDOW_TITLE "Holy Shift"
#define WINDOW_SCALE 2 // start size of the window, in view sizes
#define TARGET_FPS 60
#define VIEW_WIDTH 640 // game pixels
#define VIEW_HEIGHT 360
#define VIEW_HFOV_DEG 90.0f
#define VIEW_PITCH_DEG 14.0f // the view looks down by this angle at the start

// Paths, relative to the folder where the game starts (the project root)
#define LEVELS_DIR "levels"
#define LEVEL_FILE "level.json"
#define LEVEL_LOGIC_FILE "level_logic.png"
#define LEVEL_TEXTURE_FILE "level_texture.png"
#define COCKPIT_METRICS_PATH "assets/car_cockpit.json"
#define COCKPIT_PANORAMA_PATH "assets/car_cockpit_panorama.png"
#define COCKPIT_WHEEL_PATH "assets/car_cockpit_wheel.png"
#define SHADER_MODE7_PATH "assets/shaders/mode7.fs"
#define SHADER_COCKPIT_PATH "assets/shaders/cockpit.fs"
#define SHADER_TREES_PATH "assets/shaders/trees.fs"
#define SHADER_SHADOW_PATH "assets/shaders/shadow.fs"
#define TREE_SHEET_DIR "assets/trees" // the pictures of the trees, see level/tree_species.h
#define SOUND_SCREECH_PATH "assets/sounds/tire_screech.wav" // made by scripts/screech_loop.py
#define SOUND_GEAR_PATH "assets/sounds/gear_change.wav"     // made by scripts/gear_sound.py

// Map
#define MAP_PIXELS_PER_METER 8.0f // the road is 6 meters wide

// Road view (Mode 7)
#define CAMERA_HEIGHT 11.0f // map pixels above the road
#define FOG_START 80.0f     // distance where the ground and the trees start to fade into the fog
#define FOG_END 500.0f      // everything is the color of the fog here, and no tree is drawn further away
#define HAZE_FADE 45.0f     // rows over which the fog at the horizon changes into the sky
#define COLOR_SKY ((Color){90, 160, 230, 255})
#define COLOR_FOG ((Color){75, 120, 135, 255})    // the color of the far ground, the far trees and the horizon
#define COLOR_OUTSIDE ((Color){40, 150, 50, 255}) // color outside of the map
#define COLOR_ROAD ((Color){130, 90, 50, 255})    // color of the road in the logic map when the level file has no color

// Forest
// The forest is placed once when a level loads, in rows along both sides of the road
#define TREE_MAP_PIXELS_PER_UNIT 3.6f // size of the models in the map, a spruce_v1 is 85 map pixels high
#define TREE_HAZE_UNITS 28.0f         // height of a typical tree in model units, sets the height of the fog at the horizon
#define TREE_VIEW_MARGIN 200.0f       // trees this close to the edge of the view are still looked at
#define TREE_SCALE_MIN 0.85f          // each tree has a random scale between these values
#define TREE_SCALE_MAX 1.2f
#define TREE_SIGHT_HEIGHT 0.2f   // the picture of a tree shows it as seen at this part of its height
#define TREE_SHADOW_RADIUS 0.13f // radius of the shadow on the ground under a tree, as part of the height of the tree
#define TREE_SHADOW_SOLID 0.25f  // part of the radius of the shadow that is full of dots, outside of it the dots get fewer
#define TREE_SHADOW_ALPHA 0.3f   // opacity of the dots of the shadow
#define TREE_MAX_LOOK_UP_DEG 10.0f // no picture looks up more than this, the trees are never very near
#define TREE_NEAR 6.0f           // trees closer than this to the camera are not drawn
#define FOREST_SEED 1234u
#define FOREST_ROAD_MARGIN 14.0f // free space between the road edge and the nearest trunk
#define FOREST_SPACING 26.0f     // size of the squares that get one tree each
#define FOREST_MIN_SPACING 15.0f // two trunks are never closer than this
#define FOREST_BORDER FOG_END    // the forest goes this far past the edge of the map
#define FOREST_CELL_SIZE 128     // size of a cell of the grid that finds the trees near the camera
#define FOREST_MAX_TREES 60000
#define TREE_MAX_VISIBLE 8192 // most trees drawn in one frame

// Camera control
#define MOUSE_SENSITIVITY_DEG 0.12f // degrees for each pixel of mouse movement
#define HEAD_MAX_YAW_DEG 120.0f     // the head turns this far to the left and to the right
#define HEAD_MIN_PITCH_DEG -10.0f   // the head looks up this far, a negative angle is up
#define HEAD_MAX_PITCH_DEG 45.0f    // the head looks down this far
#define FLY_SPEED 100.0f            // map pixels each second in the fly mode

#endif
