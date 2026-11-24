#pragma once

#include "gl_functions.h"

#include <array>

constexpr int MAX_WAVE_LAYERS = 8;

// Each layer is a surface bounded by one procedural curve. Coordinates are
// aspect-corrected: y spans [-1, 1] and x spans [-aspect, aspect].
struct WaveLayerSettings {
    float baseY = 0.0f;
    float tilt = 0.0f;
    float amplitude = 0.2f;
    float frequency = 1.0f;
    float thickness = 0.12f;
    float speed = 0.1f;
    bool isReversed = false;
    bool fillsBelow = false;
    float opacity = 1.0f;
    float accent = 0.0f;
    float accentSpeed = 0.15f;
    float accentSpread = 1.2f;
    float accentSharpness = 2.0f;
    float seed = 0.0f;
    float accentOffset = 0.0f;
};

struct WaveBackgroundSettings {
    float backgroundColor[3] = {0.0f, 0.0f, 0.0f};
    float metalColor[3] = {0.86f, 0.86f, 0.88f};
    float accentPrimary[3] = {1.0f, 0.03f, 0.14f};
    float accentSecondary[3] = {1.0f, 0.30f, 0.40f};
    float glowColor[3] = {0.85f, 0.0f, 0.10f};
    float colorIntensity = 1.0f;
    float saturation = 1.0f;

    bool isAnimated = true;
    float speed = 1.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float thickness = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float flowAngleDegrees = 0.0f;
    float animationIntensity = 0.8f;

    float glowIntensity = 0.8f;
    float glowRadius = 0.08f;
    float edgeIntensity = 1.0f;
    float metallicReflection = 0.6f;
    float transparency = 0.35f;
    float shadowIntensity = 0.5f;
    float lightAngleDegrees = 115.0f;
    float vignette = 0.35f;

    float resolutionScale = 0.6f;
    int quality = 3;
    int layerCount = 5;
    float targetFps = 30.0f;

    // Field order: baseY, tilt, amplitude, frequency, thickness, speed, isReversed, fillsBelow,
    // opacity, accent, accentSpeed, accentSpread, accentSharpness, seed, accentOffset.
    std::array<WaveLayerSettings, MAX_WAVE_LAYERS> layers = {{
        // Faint ribbon far behind everything, for depth.
        {0.20f, -0.15f, 0.20f, 0.70f, 0.20f, 0.04f, true, false, 0.25f, 0.0f, 0.10f, 1.0f, 2.0f, 41.0f, 0.0f},
        // Gray sheet hanging from the top-right.
        {1.053f, -0.171f, 0.561f, 0.80f, 0.16f, 0.06f, false, false, 1.0f, 0.15f, 0.08f, 1.2f, 3.0f, 80.26f, 0.0f},
        // Large diagonal surface with the crimson rim.
        {0.648f, -0.296f, 0.528f, 0.85f, 0.10f, 0.05f, true, true, 1.0f, 1.0f, 0.12f, 1.6f, 1.5f, 73.65f, 2.05f},
        // Bottom hill whose rim turns red as it rises to the right.
        {-0.492f, 0.065f, 0.277f, 1.35f, 0.12f, 0.07f, false, true, 1.0f, 0.9f, 0.10f, 1.4f, 1.8f, 16.22f, 0.87f},
        // Translucent front sheet crossing the bottom hill.
        {-1.013f, -0.232f, 0.555f, 0.50f, 0.14f, 0.08f, true, true, 0.85f, 0.15f, 0.09f, 1.0f, 3.0f, 83.86f, 0.0f},
        {}, {}, {},
    }};
};

class WaveBackground {
public:
    WaveBackgroundSettings settings;

    bool init();
    void render(double deltaSeconds, int displayWidth, int displayHeight);
    void request_redraw() { needsRedraw = true; }
    void shutdown();

private:
    void resize_target(int width, int height);
    void upload_params();
    void draw_scene();

    GLuint program = 0;
    GLuint vertexArray = 0;
    GLuint paramsBuffer = 0;
    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    int targetWidth = 0;
    int targetHeight = 0;

    double animationSeconds = 0.0;
    double secondsSinceSceneDraw = 0.0;
    bool needsRedraw = true;
};
