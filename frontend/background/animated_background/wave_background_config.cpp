#include "wave_background_config.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>

namespace {

// Lists every persisted setting once, so saving and loading can never drift apart.
template <typename Settings, typename Visit>
void visit_settings(Settings& s, Visit&& visit) {
    visit("color.background", s.backgroundColor);
    visit("color.metal", s.metalColor);
    visit("color.accent_primary", s.accentPrimary);
    visit("color.accent_secondary", s.accentSecondary);
    visit("color.glow", s.glowColor);
    visit("color.intensity", s.colorIntensity);
    visit("color.saturation", s.saturation);

    visit("animation.enabled", s.isAnimated);
    visit("animation.speed", s.speed);
    visit("animation.amplitude", s.amplitude);
    visit("animation.frequency", s.frequency);
    visit("animation.thickness", s.thickness);
    visit("animation.offset_x", s.offsetX);
    visit("animation.offset_y", s.offsetY);
    visit("animation.flow_angle", s.flowAngleDegrees);
    visit("animation.intensity", s.animationIntensity);

    visit("lighting.glow_intensity", s.glowIntensity);
    visit("lighting.glow_radius", s.glowRadius);
    visit("lighting.edge_intensity", s.edgeIntensity);
    visit("lighting.metallic_reflection", s.metallicReflection);
    visit("lighting.transparency", s.transparency);
    visit("lighting.shadow_intensity", s.shadowIntensity);
    visit("lighting.light_angle", s.lightAngleDegrees);
    visit("lighting.vignette", s.vignette);

    visit("rendering.resolution_scale", s.resolutionScale);
    visit("rendering.quality", s.quality);
    visit("rendering.layer_count", s.layerCount);
    visit("rendering.target_fps", s.targetFps);

    for (int i = 0; i < MAX_WAVE_LAYERS; ++i) {
        auto& layer = s.layers[i];
        std::string prefix = "layer" + std::to_string(i) + ".";
        visit(prefix + "base_y", layer.baseY);
        visit(prefix + "tilt", layer.tilt);
        visit(prefix + "amplitude", layer.amplitude);
        visit(prefix + "frequency", layer.frequency);
        visit(prefix + "thickness", layer.thickness);
        visit(prefix + "speed", layer.speed);
        visit(prefix + "reversed", layer.isReversed);
        visit(prefix + "fills_below", layer.fillsBelow);
        visit(prefix + "opacity", layer.opacity);
        visit(prefix + "accent", layer.accent);
        visit(prefix + "accent_speed", layer.accentSpeed);
        visit(prefix + "accent_spread", layer.accentSpread);
        visit(prefix + "accent_sharpness", layer.accentSharpness);
        visit(prefix + "seed", layer.seed);
        visit(prefix + "accent_offset", layer.accentOffset);
    }
}

}

bool save_wave_background_settings(const WaveBackgroundSettings& settings, const char* path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    visit_settings(settings, [&file](const std::string& key, const auto& value) {
        file << key << " =";
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_array_v<std::remove_reference_t<decltype(value)>>) {
            for (float channel : value) {
                file << ' ' << channel;
            }
        } else if constexpr (std::is_same_v<Value, bool>) {
            file << ' ' << (value ? "true" : "false");
        } else {
            file << ' ' << value;
        }
        file << '\n';
    });
    return static_cast<bool>(file);
}

bool load_wave_background_settings(WaveBackgroundSettings& settings, const char* path) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        size_t separator = line.find('=');
        if (line.empty() || line[0] == '#' || separator == std::string::npos) {
            continue;
        }
        std::istringstream key(line.substr(0, separator));
        std::string trimmedKey;
        key >> trimmedKey;
        values[trimmedKey] = line.substr(separator + 1);
    }

    visit_settings(settings, [&values](const std::string& key, auto& value) {
        auto found = values.find(key);
        if (found == values.end()) {
            return;
        }
        std::istringstream text(found->second);
        using Value = std::remove_reference_t<decltype(value)>;
        if constexpr (std::is_array_v<Value>) {
            for (float& channel : value) {
                text >> channel;
            }
        } else if constexpr (std::is_same_v<Value, bool>) {
            std::string word;
            text >> word;
            value = (word == "true" || word == "1");
        } else {
            text >> value;
        }
    });
    settings.layerCount = std::clamp(settings.layerCount, 0, MAX_WAVE_LAYERS);
    return true;
}
