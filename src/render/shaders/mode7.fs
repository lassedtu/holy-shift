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
        finalColor = vec4(mix(fogColor, skyColor, smoothstep(hazeHeight, hazeHeight + hazeFade, height)), 1.0);
        return;
    }

    float depth = camHeight * front / -up;
    float side = camHeight * p.x / -up;
    vec2 right = vec2(-camDir.y, camDir.x);
    vec2 ground = camPos + camDir * depth + right * side;
    vec2 uv = ground / mapSize;

    vec3 color = outsideColor;
    if (uv.x >= 0.0 && uv.x < 1.0 && uv.y >= 0.0 && uv.y < 1.0)
    {
        color = textureLod(levelTexture, uv, 0.0).rgb;
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
    float amount = smoothstep(fog.x, fog.y, depth) * (1.0 - HEADLIGHT_CLEAR * beam);
    finalColor = vec4(mix(lit, fogColor, amount), 1.0);
}
