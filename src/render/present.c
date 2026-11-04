#include "render/present.h"

#include <math.h>

#include "core/config.h"

Rectangle PresentDestination(int window_width, int window_height)
{
    float scale = fminf((float)window_width / VIEW_WIDTH, (float)window_height / VIEW_HEIGHT);
    if (scale >= 1.0f)
    {
        scale = floorf(scale);
    }
    float width = VIEW_WIDTH * scale;
    float height = VIEW_HEIGHT * scale;
    return (Rectangle){(window_width - width) / 2.0f, (window_height - height) / 2.0f, width,
                       height};
}
