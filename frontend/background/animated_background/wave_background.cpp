#include "wave_background.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

#ifdef _WIN32
const char* SHADER_HEADER = "#version 330 core\n";
#else
const char* SHADER_HEADER = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif

const char* VERTEX_SHADER = R"GLSL(
void main() {
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* FRAGMENT_SHADER =
#include "shaders/waves.frag.glsl"
    ;

constexpr double TWO_PI = 6.283185307179586;
constexpr double DEGREES_TO_RADIANS = TWO_PI / 360.0;

// Irrational ratios keep the octaves from ever lining up again, so the motion never visibly loops.
constexpr double OCTAVE_SPEEDS[4] = {1.0, -1.618034, 2.414214, -3.302776};
constexpr double SEED_SPREADS[4] = {1.0, 2.618034, 4.236068, 6.854102};
constexpr double OCTAVE_SWAY = 0.8;

struct LayerStd140 {
    float shape[4];
    float style[4];
    float phases[4];
    float motion[4];
    float accent[4];
};

// Mirrors the BackgroundParams block in shaders/waves.frag.glsl; only vec4-sized members, so std140 adds no padding.
struct BackgroundParamsStd140 {
    float resolution[4];
    float background[4];
    float metal[4];
    float accentPrimary[4];
    float accentSecondary[4];
    float glow[4];
    float lighting[4];
    float grading[4];
    float transform[4];
    float scales[4];
    int counts[4];
    LayerStd140 layers[MAX_WAVE_LAYERS];
};
static_assert(sizeof(LayerStd140) == 80, "Layer must match std140 layout");
static_assert(sizeof(BackgroundParamsStd140) == 11 * 16 + MAX_WAVE_LAYERS * 80,
              "BackgroundParams must match std140 layout");

// Phases are reduced in double precision so the shader never sees large time values,
// which would make sin() jitter after the screen has been running for hours.
float wrap_phase(double radians) {
    return static_cast<float>(std::fmod(radians, TWO_PI));
}

void copy_color(float destination[4], const float color[3], float w) {
    destination[0] = color[0];
    destination[1] = color[1];
    destination[2] = color[2];
    destination[3] = w;
}

GLuint compile_shader(GLenum type, const char* body) {
    const char* sources[] = {SHADER_HEADER, body};
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);

    GLint isCompiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (!isCompiled) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Wave background shader failed to compile:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}

bool WaveBackground::init() {
    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (!vertexShader || !fragmentShader) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint isLinked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (!isLinked) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Wave background shader failed to link:\n%s\n", log);
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    glUniformBlockBinding(program, glGetUniformBlockIndex(program, "BackgroundParams"), 0);

    glGenBuffers(1, &paramsBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, paramsBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(BackgroundParamsStd140), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsBuffer);

    glGenVertexArrays(1, &vertexArray);
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    return true;
}

void WaveBackground::render(double deltaSeconds, int displayWidth, int displayHeight) {
    if (!program || displayWidth <= 0 || displayHeight <= 0) {
        return;
    }

    int width = std::max(1, static_cast<int>(displayWidth * settings.resolutionScale + 0.5f));
    int height = std::max(1, static_cast<int>(displayHeight * settings.resolutionScale + 0.5f));
    if (width != targetWidth || height != targetHeight) {
        resize_target(width, height);
        needsRedraw = true;
    }

    if (settings.isAnimated) {
        animationSeconds += deltaSeconds * settings.speed;
        secondsSinceSceneDraw += deltaSeconds;
        double interval = 1.0 / std::max(settings.targetFps, 1.0f);
        // The 5% slack stops vsync jitter from skipping every other frame when the target equals the refresh rate.
        if (secondsSinceSceneDraw >= interval * 0.95) {
            secondsSinceSceneDraw = std::fmod(secondsSinceSceneDraw, interval);
            needsRedraw = true;
        }
    }

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    if (needsRedraw) {
        draw_scene();
        needsRedraw = false;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, targetWidth, targetHeight, 0, 0, displayWidth, displayHeight,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void WaveBackground::shutdown() {
    glDeleteTextures(1, &colorTexture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteBuffers(1, &paramsBuffer);
    glDeleteProgram(program);
    colorTexture = framebuffer = vertexArray = paramsBuffer = program = 0;
}

void WaveBackground::resize_target(int width, int height) {
    targetWidth = width;
    targetHeight = height;

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Wave background framebuffer is incomplete (%dx%d)\n", width, height);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void WaveBackground::upload_params() {
    const WaveBackgroundSettings& s = settings;
    BackgroundParamsStd140 params = {};

    params.resolution[0] = static_cast<float>(targetWidth);
    params.resolution[1] = static_cast<float>(targetHeight);
    params.resolution[2] = 1.0f / targetWidth;
    params.resolution[3] = 1.0f / targetHeight;

    copy_color(params.background, s.backgroundColor, s.vignette);
    copy_color(params.metal, s.metalColor, s.metallicReflection);
    copy_color(params.accentPrimary, s.accentPrimary, s.edgeIntensity);
    copy_color(params.accentSecondary, s.accentSecondary, s.transparency);
    copy_color(params.glow, s.glowColor, s.glowIntensity);

    params.lighting[0] = s.glowRadius;
    params.lighting[1] = s.shadowIntensity;
    params.lighting[2] = static_cast<float>(s.lightAngleDegrees * DEGREES_TO_RADIANS);
    params.lighting[3] = s.animationIntensity;

    params.grading[0] = s.colorIntensity;
    params.grading[1] = s.saturation;

    double flowAngle = s.flowAngleDegrees * DEGREES_TO_RADIANS;
    params.transform[0] = s.offsetX;
    params.transform[1] = s.offsetY;
    params.transform[2] = static_cast<float>(std::cos(flowAngle));
    params.transform[3] = static_cast<float>(std::sin(flowAngle));

    params.scales[0] = s.amplitude;
    params.scales[1] = s.frequency;
    params.scales[2] = s.thickness;

    params.counts[0] = std::clamp(s.layerCount, 0, MAX_WAVE_LAYERS);
    params.counts[1] = std::clamp(s.quality + 1, 2, 4);

    for (int i = 0; i < MAX_WAVE_LAYERS; ++i) {
        const WaveLayerSettings& layer = s.layers[i];
        LayerStd140& packed = params.layers[i];
        double seed = layer.seed;
        double layerSeconds = animationSeconds * layer.speed * (layer.isReversed ? -1.0 : 1.0);
        double accentSeconds = animationSeconds * layer.accentSpeed;

        packed.shape[0] = layer.baseY;
        packed.shape[1] = layer.tilt;
        packed.shape[2] = layer.amplitude;
        packed.shape[3] = layer.frequency;

        packed.style[0] = layer.thickness;
        packed.style[1] = layer.fillsBelow ? -1.0f : 1.0f;
        packed.style[2] = layer.opacity;
        packed.style[3] = layer.accent;

        // The two broad octaves only sway around their rest phase so the composition stays
        // recognisable; the fine octaves scroll continuously in the layer's direction.
        for (int k = 0; k < 4; ++k) {
            double offset = k < 2
                ? s.animationIntensity * OCTAVE_SWAY * std::sin(layerSeconds * OCTAVE_SPEEDS[k] + seed)
                : layerSeconds * OCTAVE_SPEEDS[k];
            packed.phases[k] = wrap_phase(seed * SEED_SPREADS[k] + offset);
        }

        packed.motion[0] = wrap_phase(seed * 1.9 + layerSeconds * 0.61);
        packed.motion[1] = wrap_phase(seed * 0.7 + layerSeconds * 0.38);
        packed.motion[2] = wrap_phase(accentSeconds);
        packed.motion[3] = wrap_phase(seed * 2.3 + accentSeconds * 1.7);

        packed.accent[0] = layer.accentSpread;
        packed.accent[1] = layer.accentSharpness;
        packed.accent[2] = wrap_phase(seed);
        packed.accent[3] = layer.accentOffset;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, paramsBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void WaveBackground::draw_scene() {
    upload_params();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, targetWidth, targetHeight);
    glUseProgram(program);
    glBindVertexArray(vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}
