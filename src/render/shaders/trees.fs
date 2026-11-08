#version 330

// Draw a tree sprite and fade it into the color of the fog
// The red channel of the tint is the amount of fog from 0 to 1, the green channel the light of the headlights

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec3 fogColor;
uniform vec3 tint; // the light of the time of day

const vec3 HEADLIGHT_COLOR = vec3(1.0, 0.95, 0.82);
const float HEADLIGHT_CLEAR = 1.0; // the same as in mode7.fs

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.5)
    {
        discard;
    }
    vec3 lit = mix(texel.rgb * tint, texel.rgb * HEADLIGHT_COLOR, fragColor.g);
    finalColor = vec4(mix(lit, fogColor, fragColor.r * (1.0 - HEADLIGHT_CLEAR * fragColor.g)), 1.0);
}
