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
    float amount = smoothstep(fog.x, fog.y, depth);
    finalColor = vec4(mix(color, fogColor, amount), 1.0);
}
