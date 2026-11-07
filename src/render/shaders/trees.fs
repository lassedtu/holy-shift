#version 330

// Draw a tree sprite and fade it into the color of the fog
// The red channel of the tint is the amount of fog from 0 to 1

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec3 fogColor;

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.5)
    {
        discard;
    }
    finalColor = vec4(mix(texel.rgb, fogColor, fragColor.r), 1.0);
}
