R"GLSL(
// The #version line and precision qualifiers are prepended at load time per platform.

const int MAX_LAYERS = 8;
const float OCTAVE_WEIGHT[4] = float[4](1.0, 0.42, 0.16, 0.07);
const float OCTAVE_MULTIPLIER[4] = float[4](1.0, 2.13, 3.71, 5.97);
const float WARP_AMOUNT = 0.12;
const float WARP_FREQUENCY = 0.9;

struct Layer {
    vec4 shape;   // baseY, tilt, amplitude, frequency
    vec4 style;   // thickness, side (+1 fills above, -1 fills below), opacity, accent amount
    vec4 phases;  // phase of each octave, already wrapped to [0, 2pi) on the CPU
    vec4 motion;  // breathe phase, warp phase, accent travel phase, accent pulse phase
    vec4 accent;  // spread along the contour, sharpness, seed, accent offset
};

layout(std140) uniform BackgroundParams {
    vec4 resolution;      // width, height, 1/width, 1/height
    vec4 background;      // rgb, vignette
    vec4 metal;           // rgb, metallic reflection
    vec4 accentPrimary;   // rgb, edge intensity
    vec4 accentSecondary; // rgb, transparency
    vec4 glow;            // rgb, glow intensity
    vec4 lighting;        // glow radius, shadow intensity, light angle (radians), animation intensity
    vec4 grading;         // color intensity, saturation, unused, unused
    vec4 transform;       // offset x, offset y, cos(flow angle), sin(flow angle)
    vec4 scales;          // amplitude, frequency, thickness, unused
    ivec4 counts;         // layer count, octave count, unused, unused
    Layer layers[MAX_LAYERS];
};

out vec4 fragColor;

// Returns the curve height in x and its slope dy/dx in y.
vec2 evaluate_curve(Layer layer, float x, int octaveCount) {
    float intensity = lighting.w;
    float warpedX = x + WARP_AMOUNT * sin(WARP_FREQUENCY * x + layer.motion.y);
    float warpSlope = 1.0 + WARP_AMOUNT * WARP_FREQUENCY * cos(WARP_FREQUENCY * x + layer.motion.y);
    float frequency = layer.shape.w * scales.y;
    float amplitude = layer.shape.z * scales.x;

    float height = layer.shape.x + layer.shape.y * x
                 + 0.04 * intensity * sin(layer.motion.y + 1.3);
    float slope = layer.shape.y;
    for (int k = 0; k < 4; ++k) {
        if (k >= octaveCount) break;
        float breathe = 1.0 + 0.2 * intensity * sin(layer.motion.x + float(k) * 1.7);
        float weight = amplitude * OCTAVE_WEIGHT[k] * breathe;
        float octaveFrequency = OCTAVE_MULTIPLIER[k] * frequency;
        float angle = octaveFrequency * warpedX + layer.phases[k];
        height += weight * sin(angle);
        slope += weight * octaveFrequency * cos(angle) * warpSlope;
    }
    return vec2(height, slope);
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 uv = gl_FragCoord.xy * resolution.zw;
    float aspect = resolution.x * resolution.w;
    vec2 centered = (uv * 2.0 - 1.0) * vec2(aspect, 1.0);
    centered -= transform.xy;
    vec2 p = vec2(transform.z * centered.x - transform.w * centered.y,
                  transform.w * centered.x + transform.z * centered.y);

    float pixelSize = 2.0 * resolution.w;
    vec2 lightDirection = vec2(cos(lighting.z), sin(lighting.z));
    vec3 color = background.rgb;

    for (int i = 0; i < MAX_LAYERS; ++i) {
        if (i >= counts.x) break;
        Layer layer = layers[i];

        vec2 curve = evaluate_curve(layer, p.x, counts.y);
        float side = layer.style.y;
        float slopeLength = sqrt(1.0 + curve.y * curve.y);
        float signedDistance = side * (p.y - curve.x) / slopeLength;

        float thickness = max(layer.style.x * scales.z, 1e-3);
        float opacity = layer.style.z;
        float coverage = smoothstep(-pixelSize, pixelSize, signedDistance);
        float inside = max(signedDistance, 0.0);

        float shadow = lighting.y * 0.6 * exp(-abs(signedDistance) / (thickness * 0.4));
        color *= 1.0 - shadow * opacity;
        color *= 1.0 - coverage * opacity * (1.0 - accentSecondary.a);

        vec2 outwardNormal = -side * vec2(-curve.y, 1.0) / slopeLength;
        float facing = 0.5 + 0.5 * dot(outwardNormal, lightDirection);
        float sheen = 0.75 + 0.25 * sin(p.x * 1.3 + layer.accent.z);
        float reflection = mix(1.0, 1.6 * facing * facing * facing * sheen, metal.a);

        float rimLight = 0.4 * exp(-inside / thickness) + 0.6 * exp(-inside / (thickness * 0.15));

        float travel = 0.5 + 0.5 * sin(p.x * layer.accent.x - layer.motion.z + layer.accent.w);
        float pulse = 0.65 + 0.35 * sin(layer.motion.w);
        float accentMask = pow(travel, max(layer.accent.y, 0.01)) * pulse * layer.style.w;
        vec3 accentColor = mix(accentPrimary.rgb, accentSecondary.rgb,
                               0.5 + 0.5 * sin(p.x * 0.8 + layer.motion.z));

        vec3 rimColor = mix(metal.rgb * reflection, accentColor, clamp(accentMask, 0.0, 1.0));
        color += rimColor * rimLight * accentPrimary.a * coverage * opacity;

        float glowRadius = max(lighting.x, 1e-3);
        color += glow.rgb * glow.a * accentMask * exp(-abs(signedDistance) / glowRadius) * opacity;
    }

    float vignette = smoothstep(0.6, 1.9, length(uv * 2.0 - 1.0) * 1.2);
    color *= 1.0 - background.a * vignette;
    color = mix(vec3(luminance(color)), color, grading.y) * grading.x;

    float dither = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    color += (dither - 0.5) / 255.0;

    fragColor = vec4(color, 1.0);
}
)GLSL"
