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
#define VIEW_PITCH_DEG 14.0f // the view looks down by this angle

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

// Road view (Mode 7)
#define CAMERA_HEIGHT 11.0f // map pixels above the road
#define FOG_START 500.0f    // distance where the road starts to fade into the sky
#define FOG_END 1600.0f
#define COLOR_SKY ((Color){90, 160, 230, 255})
#define COLOR_OUTSIDE ((Color){40, 150, 50, 255}) // color outside of the map

// Camera control
#define MOUSE_SENSITIVITY_DEG 0.12f // degrees for each pixel of mouse movement
#define HEAD_MAX_YAW_DEG 120.0f     // the head turns this far to the left and to the right
#define FLY_SPEED 100.0f            // map pixels each second in the fly mode

#endif
