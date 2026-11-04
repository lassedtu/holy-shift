// Cockpit panorama demo. Uses raylib.
//
// Left/Right: Turn the head. The limit is 120 degrees in each direction.
// A/D: Turn the steering wheel. Release the key to return the wheel to centre.
// F/F11: Toggle fullscreen mode. The display keeps a 16:9 aspect ratio.
// Esc: Quit.
//
// The panorama is an equirectangular strip. The fragment shader creates a
// perspective view with a 90 degree field of view. It applies the camera tilt
// and head yaw, then samples the strip along each pixel's view ray. This keeps
// the image from stretching at the edges. The shader composites the wheel in
// panorama space so the wheel and cabin use the same projection.
//
// Run from the project root. The program loads images from assets/:
//   make -C demo run
//
// To save a screenshot without keyboard input, run:
//   ./demo/cockpit_demo --yaw 60 --steer 0.5 --shot /tmp/shot.png
// Add --fullscreen to start in fullscreen mode.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

// Values from assets/car_cockpit.json. The panorama uses scale 2.
#define VIEW_W 1280 // 640 game pixels at scale 2.
#define VIEW_H 720  // 360 game pixels at scale 2.
#define VIEW_HFOV_DEG 90.0f
#define VIEW_PITCH_DEG 14.0f // Downward view angle.
#define PANO_W 3910
#define PANO_H 1061
#define PX_PER_DEG 11.170107212763712f
#define YAW_MIN_DEG (-175.0f)
#define LAT_TOP_DEG 25.0f
#define MAX_HEAD_YAW_DEG 120.0f

#define WHEEL_X 1783 // Left edge of the wheel frames in panorama pixels.
#define WHEEL_Y 634
#define WHEEL_FRAME_W 344
#define WHEEL_FRAME_H 243
#define WHEEL_FRAMES 9
static const float WHEEL_ANGLES_DEG[WHEEL_FRAMES] = {-90, -70, -45, -22, 0,
                                                     22, 45, 70, 90};

#define HEAD_TURN_SPEED 90.0f   // Degrees per second.
#define STEER_SPEED 2.5f        // Time to full lock: 0.4 seconds.
#define STEER_RETURN_SPEED 3.5f // Wheel return speed.

static const char *FRAGMENT_SHADER =
    "#version 330\n"
    "uniform sampler2D panorama;\n"
    "uniform sampler2D wheelTex;\n"
    "uniform vec2 center;\n"     // Viewport centre in window pixels. Y points up.
    "uniform float focal;\n"     // Focal length in pixels.
    "uniform float headYaw;\n"   // Head angle in degrees. Positive is right.
    "uniform float pitchDown;\n" // Downward angle in degrees.
    "uniform vec2 panoSize;\n"
    "uniform float ppd;\n" // Panorama pixels per degree.
    "uniform float yawMin;\n"
    "uniform float latTop;\n"
    "uniform vec4 wheelRect;\n" // x, y, width, and height in panorama pixels.
    "uniform float wheelFrame;\n"
    "uniform vec2 wheelAtlasSize;\n"
    "out vec4 finalColor;\n"
    "const float DEG = 0.017453292519943295;\n"
    "void main() {\n"
    "    vec2 p = gl_FragCoord.xy - center;\n" // Y points up.
    "    vec3 d = normalize(vec3(p.x, p.y, focal));\n"
    "    float th = pitchDown * DEG;\n"
    "    vec3 r = vec3(d.x, d.y * cos(th) - d.z * sin(th),\n"
    "                  d.y * sin(th) + d.z * cos(th));\n"
    "    float yaw = headYaw + atan(r.x, r.z) / DEG;\n"
    "    float lat = asin(clamp(r.y, -1.0, 1.0)) / DEG;\n"
    "    vec2 pp = vec2((yaw - yawMin) * ppd, (latTop - lat) * ppd);\n"
    "    vec2 uv = pp / panoSize;\n"
    "    vec4 base = vec4(0.0);\n"
    "    if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)\n"
    "        base = textureLod(panorama, uv, 0.0);\n"
    "    vec4 w = vec4(0.0);\n"
    "    vec2 wp = pp - wheelRect.xy;\n"
    "    if (wp.x >= 0.0 && wp.y >= 0.0 && wp.x < wheelRect.z &&\n"
    "        wp.y < wheelRect.w)\n"
    "        w = textureLod(wheelTex, vec2((wheelFrame * wheelRect.z + wp.x) /\n"
    "                                      wheelAtlasSize.x,\n"
    "                                      wp.y / wheelAtlasSize.y), 0.0);\n"
    // Composite the wheel over the cabin with straight alpha.
    "    float a = w.a + base.a * (1.0 - w.a);\n"
    "    vec3 rgb = a > 0.0 ? (w.rgb * w.a + base.rgb * base.a * (1.0 - w.a)) / a\n"
    "                       : vec3(0.0);\n"
    "    finalColor = vec4(rgb, a);\n"
    "}\n";

static float MoveToward(float value, float target, float max_delta)
{
    if (fabsf(target - value) <= max_delta)
        return target;
    return value + (target > value ? max_delta : -max_delta);
}

// Return the wheel frame with the closest steering angle.
static int NearestWheelFrame(float angle_deg)
{
    int best = 0;
    for (int i = 1; i < WHEEL_FRAMES; i++)
    {
        if (fabsf(WHEEL_ANGLES_DEG[i] - angle_deg) <
            fabsf(WHEEL_ANGLES_DEG[best] - angle_deg))
        {
            best = i;
        }
    }
    return best;
}

static void SetFloat(Shader s, const char *name, float v)
{
    SetShaderValue(s, GetShaderLocation(s, name), &v, SHADER_UNIFORM_FLOAT);
}

// Return the largest centred 16:9 viewport that fits the window.
typedef struct
{
    int x, y, w, h;
} Viewport;

static Viewport FitViewport(int win_w, int win_h)
{
    float scale = fminf((float)win_w / VIEW_W, (float)win_h / VIEW_H);
    Viewport v;
    v.w = (int)(VIEW_W * scale);
    v.h = (int)(VIEW_H * scale);
    v.x = (win_w - v.w) / 2;
    v.y = (win_h - v.h) / 2;
    return v;
}

static void SetVec2(Shader s, const char *name, float x, float y)
{
    float v[2] = {x, y};
    SetShaderValue(s, GetShaderLocation(s, name), v, SHADER_UNIFORM_VEC2);
}

int main(int argc, char **argv)
{
    float yaw = 0.0f;   // Degrees. Positive is right.
    float steer = 0.0f; // -1 is full left. 1 is full right.
    const char *shot = NULL;
    bool start_fullscreen = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--fullscreen"))
            start_fullscreen = true;
        else if (i + 1 < argc && !strcmp(argv[i], "--yaw"))
            yaw = (float)atof(argv[++i]);
        else if (i + 1 < argc && !strcmp(argv[i], "--steer"))
            steer = (float)atof(argv[++i]);
        else if (i + 1 < argc && !strcmp(argv[i], "--shot"))
            shot = argv[++i];
    }

    InitWindow(VIEW_W, VIEW_H, "Cockpit panorama demo");
    if (start_fullscreen)
        ToggleBorderlessWindowed();
    SetTargetFPS(60);

    Texture2D panorama = LoadTexture("assets/car_cockpit_panorama.png");
    Texture2D wheel = LoadTexture("assets/car_cockpit_wheel.png");
    if (panorama.id == 0 || wheel.id == 0)
    {
        fprintf(stderr, "Could not load the cockpit images. "
                        "Run this from the project root.\n");
        CloseWindow();
        return 1;
    }
    SetTextureFilter(panorama, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(wheel, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(panorama, TEXTURE_WRAP_CLAMP);
    SetTextureWrap(wheel, TEXTURE_WRAP_CLAMP);

    Shader shader = LoadShaderFromMemory(NULL, FRAGMENT_SHADER);
    float wheel_rect[4] = {WHEEL_X, WHEEL_Y, WHEEL_FRAME_W, WHEEL_FRAME_H};
    SetFloat(shader, "pitchDown", VIEW_PITCH_DEG);
    SetVec2(shader, "panoSize", PANO_W, PANO_H);
    SetFloat(shader, "ppd", PX_PER_DEG);
    SetFloat(shader, "yawMin", YAW_MIN_DEG);
    SetFloat(shader, "latTop", LAT_TOP_DEG);
    SetShaderValue(shader, GetShaderLocation(shader, "wheelRect"), wheel_rect,
                   SHADER_UNIFORM_VEC4);
    SetVec2(shader, "wheelAtlasSize", (float)wheel.width, (float)wheel.height);
    int panorama_loc = GetShaderLocation(shader, "panorama");
    int wheel_loc = GetShaderLocation(shader, "wheelTex");

    const Color background = (Color){90, 160, 230, 255};
    int frames_drawn = 0;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        float head = (float)(IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT));
        yaw += head * HEAD_TURN_SPEED * dt;
        if (yaw > MAX_HEAD_YAW_DEG)
            yaw = MAX_HEAD_YAW_DEG;
        if (yaw < -MAX_HEAD_YAW_DEG)
            yaw = -MAX_HEAD_YAW_DEG;

        if (!shot)
        {
            float input = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
            if (input != 0.0f)
            {
                steer = MoveToward(steer, input, STEER_SPEED * dt);
            }
            else
            {
                steer = MoveToward(steer, 0.0f, STEER_RETURN_SPEED * dt);
            }
        }
        int frame = NearestWheelFrame(steer * 90.0f);

        if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_F11))
        {
            ToggleBorderlessWindowed();
        }

        // Draw the largest centred 16:9 image that fits the window.
        // Scale the focal length with the viewport to keep the 90 degree FOV.
        int win_w = GetScreenWidth(), win_h = GetScreenHeight();
        Viewport vp = FitViewport(win_w, win_h);
        SetFloat(shader, "focal", (vp.w / 2.0f) / tanf(VIEW_HFOV_DEG * 0.5f * DEG2RAD));
        SetVec2(shader, "center", vp.x + vp.w / 2.0f,
                win_h - (vp.y + vp.h / 2.0f));

        SetFloat(shader, "headYaw", yaw);
        SetFloat(shader, "wheelFrame", (float)frame);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(vp.x, vp.y, vp.w, vp.h, background);
        BeginShaderMode(shader);
        // BeginShaderMode starts a new draw batch and resets custom samplers.
        // Set the sampler textures after BeginShaderMode on every frame.
        SetShaderValueTexture(shader, panorama_loc, panorama);
        SetShaderValueTexture(shader, wheel_loc, wheel);
        DrawRectangle(vp.x, vp.y, vp.w, vp.h, WHITE);
        EndShaderMode();
        DrawText(TextFormat("yaw %+.0f  wheel %+.0f", yaw,
                            WHEEL_ANGLES_DEG[frame]),
                 vp.x + 10, vp.y + 10, 20, WHITE);
        EndDrawing();

        if (shot && ++frames_drawn == (start_fullscreen ? 30 : 3))
        {
            Image img = LoadImageFromScreen();
            ExportImage(img, shot);
            UnloadImage(img);
            break;
        }
    }

    UnloadShader(shader);
    UnloadTexture(wheel);
    UnloadTexture(panorama);
    CloseWindow();
    return 0;
}
