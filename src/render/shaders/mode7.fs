#version 330

// Draw the ground of the level as a flat plane that the camera looks at
// Pixels above the horizon show the sky, which has the color of the fog at the horizon

uniform sampler2D levelTexture;
uniform vec2 resolution; // size of the view in pixels
uniform vec2 mapSize;    // size of the level texture in map pixels
uniform vec2 camPos;     // position of the camera in the map
uniform vec2 camDir;     // direction of the camera in the map
uniform float camHeight;
uniform float focal;     // focal length in pixels
uniform float pitch;     // radians, a positive angle is down
uniform vec3 skyColor;
uniform vec3 fogColor;
uniform float hazeHeight; // rows over the horizon where the sky is still the color of the fog
uniform float hazeFade;   // rows over which the fog changes into the sky
uniform vec3 outsideColor;
uniform vec2 fog;        // start and end distance of the fade
uniform vec3 tint;       // the light of the time of day on the ground
uniform float headlights; // from 0 to 1
uniform vec2 carDir;     // the direction of the car, the headlights point this way
uniform vec2 headlightReach; // the headlights are full until x and fade out until y
uniform float headlightSpread; // the tangent of the half angle of the cone of one headlight
uniform vec2 headlightToe;     // the cosine and the sine of the angle that each headlight points out to its side
uniform float headlightSpacing; // from the middle of the car to each headlight
uniform vec2 headlightCenter;   // the point between the two headlights, x to the front and y to the right of the eye
uniform float wetness;   // from 0 to 1, how wet the road is (rain)
uniform float time;      // seconds, for the ripples and the glints
uniform vec3 glintColor; // the sparkle of the sun on water
uniform float glintRate; // from 0 to 1, how often the water sparkles (less without sun)
uniform vec2 sunDir;     // the direction of the sun on the map

// The materials in the alpha channel of the level texture (level/ground.h)
const float MATERIAL_PUDDLE = 64.0 / 255.0;
const float MATERIAL_ROAD = 192.0 / 255.0;

// Water reflects the sky: the reflected ray reads the color of the sky, with ripples, in a few dithered bands
const float REFLECT_BANDS = 4.0;
const float RIPPLE_ROWS = 3.0;      // how far the ripples move the reflected horizon, in rows of the view
const float FRESNEL_ANGLE = 0.45;   // radians below the horizon where the water shows only a little reflection
const float WATER_DARK = 0.55;      // the color of water under the reflection is this much darker
const float WET_DARK = 0.3;         // wet gravel is this much darker
const float WET_REFLECT = 0.35;     // the road reflects this much of what water reflects
const float GLINT_RATE = 0.06;      // part of the pixels of water that sparkle when the view faces the sun

const int BAYER[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);

float Bayer()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy) & 3;
    return (float(BAYER[pixel.y * 4 + pixel.x]) + 0.5) / 16.0;
}

float Hash(vec2 v)
{
    return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453);
}

// The color of the sky at a height over the horizon, in rows of the view
vec3 SkyAt(float height)
{
    return mix(fogColor, skyColor, smoothstep(hazeHeight, hazeHeight + hazeFade, height));
}

const float HEADLIGHT_BANDS = 4.0;
const vec3 HEADLIGHT_COLOR = vec3(1.0, 0.95, 0.82);
// The light of one headlight at a point: brightest in the middle of its cone, less to the sides and far away
float Headlight(vec2 offset, vec2 dir)
{
    float along = dot(offset, dir);
    if (along <= 0.0)
    {
        return 0.0;
    }
    float across = abs(dot(offset, vec2(-dir.y, dir.x)));
    float cone = 1.0 - smoothstep(0.0, 1.0, across / (along * headlightSpread));
    return cone * (1.0 - smoothstep(headlightReach.x, headlightReach.y, along));
}

const float HEADLIGHT_CLEAR = 1.0; // the headlights take away this part of the fog, at night the fog is darkness

out vec4 finalColor;

void main()
{
    vec2 p = gl_FragCoord.xy - 0.5 * resolution;
    float up = p.y * cos(pitch) - focal * sin(pitch);
    float front = p.y * sin(pitch) + focal * cos(pitch);
    if (up > -1e-4)
    {
        // The far trees fade into the fog, so the sky near the horizon has the color of the fog
        float height = focal * up / length(vec2(p.x, front));
        finalColor = vec4(SkyAt(height), 1.0);
        return;
    }

    float depth = camHeight * front / -up;
    float side = camHeight * p.x / -up;
    vec2 right = vec2(-camDir.y, camDir.x);
    vec2 ground = camPos + camDir * depth + right * side;
    vec2 uv = ground / mapSize;

    vec3 color = outsideColor;
    float material = 1.0;
    if (uv.x >= 0.0 && uv.x < 1.0 && uv.y >= 0.0 && uv.y < 1.0)
    {
        vec4 texel = textureLod(levelTexture, uv, 0.0);
        color = texel.rgb;
        material = texel.a;
    }
    // The headlights light a cone in front of the car, in hard bands so the light keeps the pixel look
    vec3 lit = color * tint;
    float beam = 0.0;
    if (headlights > 0.0)
    {
        // Two headlights, one on each side of the car, each turned a little out to its side
        vec2 right = vec2(-carDir.y, carDir.x);
        vec2 left_dir = carDir * headlightToe.x - right * headlightToe.y;
        vec2 right_dir = carDir * headlightToe.x + right * headlightToe.y;
        vec2 offset = ground - (camPos + carDir * headlightCenter.x + right * headlightCenter.y);
        // The brighter of the two, so there is a darker gap between the two cones
        float light = max(Headlight(offset + right * headlightSpacing, left_dir),
                          Headlight(offset - right * headlightSpacing, right_dir));
        beam = floor(light * headlights * HEADLIGHT_BANDS + 0.5) / HEADLIGHT_BANDS;
        lit = mix(lit, color * HEADLIGHT_COLOR, beam);
    }

    // Water and the wet road: a darker color under a reflection of the sky
    bool is_water = material < (MATERIAL_PUDDLE + MATERIAL_ROAD) * 0.5;
    bool is_road = !is_water && material < 0.9;
    float water = is_water ? 1.0 : (is_road ? wetness * WET_REFLECT : 0.0);
    if (water > 0.0)
    {
        float distance = length(vec2(p.x, front));
        float wave = sin(ground.x * 0.11 + time * 1.9) * sin(ground.y * 0.09 - time * 1.4);
        vec3 reflected = SkyAt(focal * -up / distance + wave * RIPPLE_ROWS);
        float steep = atan(-up, distance);
        float fresnel = clamp(1.0 - steep / FRESNEL_ANGLE, 0.2, 1.0) * water;
        fresnel = floor(fresnel * REFLECT_BANDS + Bayer()) / REFLECT_BANDS;
        lit *= is_water ? WATER_DARK : 1.0 - WET_DARK * wetness;
        lit = mix(lit, reflected, fresnel);

        // Glints: a few pixels sparkle, more when the view faces the sun
        vec2 view = normalize(ground - camPos);
        float sun = max(dot(view, sunDir), 0.0);
        float sparkle = Hash(floor(ground * 0.7) + floor(time * 6.0));
        if (sparkle > 1.0 - GLINT_RATE * glintRate * sun * sun * water)
        {
            lit = glintColor;
        }
    }

    float amount = smoothstep(fog.x, fog.y, depth) * (1.0 - HEADLIGHT_CLEAR * beam);
    finalColor = vec4(mix(lit, fogColor, amount), 1.0);
}
