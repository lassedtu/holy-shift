#version 330

// Draw the shadow of a tree as dots, with more dots near the middle and fewer dots at the edge
// The alpha of the picture is the strength of the shadow, the alpha of the tint is the fog
// The dots are one pixel of the view each, in an ordered pattern

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float darkness; // opacity of the dots

out vec4 finalColor;

const int pattern[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);

void main()
{
    float strength = texture(texture0, fragTexCoord).a * fragColor.a;
    ivec2 pixel = ivec2(gl_FragCoord.xy) & 3;
    float threshold = (float(pattern[pixel.y * 4 + pixel.x]) + 0.5) / 16.0;
    if (strength <= threshold)
    {
        discard;
    }
    finalColor = vec4(0.0, 0.0, 0.0, darkness);
}
