#version 330

// Draw the cabin from the panorama strip, with the steering wheel on top
// The shader makes a perspective view of the strip for each pixel, so the picture does not stretch
// The wheel frame is in panorama space, so it moves and turns with the cabin

uniform sampler2D panorama;
uniform sampler2D wheelTex;
uniform vec2 center;     // centre of the view in window pixels, y points up
uniform float focal;     // focal length in pixels
uniform float headYaw;   // degrees, positive is right
uniform float pitchDown; // degrees
uniform vec2 panoSize;
uniform float ppd;       // panorama pixels per degree
uniform float yawMin;    // yaw at the left edge of the panorama
uniform float latTop;    // elevation at the top of the panorama
uniform vec4 wheelRect;  // x, y, width and height of a wheel frame in panorama pixels
uniform float wheelFrame;
uniform vec2 wheelAtlasSize;

out vec4 finalColor;

const float DEG = 0.017453292519943295;

void main()
{
    vec2 p = gl_FragCoord.xy - center;
    vec3 d = normalize(vec3(p.x, p.y, focal));
    float th = pitchDown * DEG;
    vec3 r = vec3(d.x, d.y * cos(th) - d.z * sin(th), d.y * sin(th) + d.z * cos(th));
    float yaw = headYaw + atan(r.x, r.z) / DEG;
    float lat = asin(clamp(r.y, -1.0, 1.0)) / DEG;
    vec2 pp = vec2((yaw - yawMin) * ppd, (latTop - lat) * ppd);

    vec4 base = vec4(0.0);
    vec2 uv = pp / panoSize;
    if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
    {
        base = textureLod(panorama, uv, 0.0);
    }

    vec4 wheel = vec4(0.0);
    vec2 wp = pp - wheelRect.xy;
    if (wp.x >= 0.0 && wp.y >= 0.0 && wp.x < wheelRect.z && wp.y < wheelRect.w)
    {
        vec2 wuv = vec2((wheelFrame * wheelRect.z + wp.x) / wheelAtlasSize.x,
                        wp.y / wheelAtlasSize.y);
        wheel = textureLod(wheelTex, wuv, 0.0);
    }

    // Put the wheel over the cabin, with straight alpha
    float a = wheel.a + base.a * (1.0 - wheel.a);
    vec3 rgb = vec3(0.0);
    if (a > 0.0)
    {
        rgb = (wheel.rgb * wheel.a + base.rgb * base.a * (1.0 - wheel.a)) / a;
    }
    finalColor = vec4(rgb, a);
}
