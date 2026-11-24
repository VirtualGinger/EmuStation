#include "wave_background_panel.h"

#include "wave_background_config.h"

#include "imgui.h"

#include <string>

bool draw_wave_background_panel(WaveBackgroundSettings& settings, bool* isOpen) {
    static std::string status;
    bool isChanged = false;

    ImGui::SetNextWindowSize(ImVec2(900.0f, 1000.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Background Settings (F1)", isOpen)) {
        ImGui::End();
        return false;
    }

    if (ImGui::Button("Save")) {
        status = save_wave_background_settings(settings, WAVE_BACKGROUND_CONFIG_PATH)
                     ? "Saved to " : "Could not write ";
        status += WAVE_BACKGROUND_CONFIG_PATH;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        isChanged = load_wave_background_settings(settings, WAVE_BACKGROUND_CONFIG_PATH);
        status = isChanged ? "Loaded " : "Could not read ";
        status += WAVE_BACKGROUND_CONFIG_PATH;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        settings = WaveBackgroundSettings{};
        status = "Reset to defaults";
        isChanged = true;
    }
    if (!status.empty()) {
        ImGui::TextUnformatted(status.c_str());
    }

    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        isChanged |= ImGui::ColorEdit3("Background", settings.backgroundColor);
        isChanged |= ImGui::ColorEdit3("Metal", settings.metalColor);
        isChanged |= ImGui::ColorEdit3("Accent primary", settings.accentPrimary);
        isChanged |= ImGui::ColorEdit3("Accent secondary", settings.accentSecondary);
        isChanged |= ImGui::ColorEdit3("Glow", settings.glowColor);
        isChanged |= ImGui::SliderFloat("Intensity", &settings.colorIntensity, 0.0f, 2.0f);
        isChanged |= ImGui::SliderFloat("Saturation", &settings.saturation, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        isChanged |= ImGui::Checkbox("Animated", &settings.isAnimated);
        isChanged |= ImGui::SliderFloat("Speed", &settings.speed, 0.0f, 5.0f);
        isChanged |= ImGui::SliderFloat("Motion intensity", &settings.animationIntensity, 0.0f, 3.0f);
        isChanged |= ImGui::SliderFloat("Amplitude", &settings.amplitude, 0.0f, 3.0f);
        isChanged |= ImGui::SliderFloat("Frequency", &settings.frequency, 0.1f, 3.0f);
        isChanged |= ImGui::SliderFloat("Thickness", &settings.thickness, 0.1f, 4.0f);
        isChanged |= ImGui::SliderFloat("Offset X", &settings.offsetX, -2.0f, 2.0f);
        isChanged |= ImGui::SliderFloat("Offset Y", &settings.offsetY, -2.0f, 2.0f);
        isChanged |= ImGui::SliderFloat("Flow angle", &settings.flowAngleDegrees, -180.0f, 180.0f, "%.0f deg");
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        isChanged |= ImGui::SliderFloat("Glow intensity", &settings.glowIntensity, 0.0f, 3.0f);
        isChanged |= ImGui::SliderFloat("Glow radius", &settings.glowRadius, 0.005f, 0.5f);
        isChanged |= ImGui::SliderFloat("Edge intensity", &settings.edgeIntensity, 0.0f, 3.0f);
        isChanged |= ImGui::SliderFloat("Metallic reflection", &settings.metallicReflection, 0.0f, 1.0f);
        isChanged |= ImGui::SliderFloat("Transparency", &settings.transparency, 0.0f, 1.0f);
        isChanged |= ImGui::SliderFloat("Shadow intensity", &settings.shadowIntensity, 0.0f, 1.0f);
        isChanged |= ImGui::SliderFloat("Light angle", &settings.lightAngleDegrees, -180.0f, 180.0f, "%.0f deg");
        isChanged |= ImGui::SliderFloat("Vignette", &settings.vignette, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        isChanged |= ImGui::SliderFloat("Resolution scale", &settings.resolutionScale, 0.25f, 1.5f);
        isChanged |= ImGui::SliderInt("Quality", &settings.quality, 1, 3);
        isChanged |= ImGui::SliderInt("Layers", &settings.layerCount, 0, MAX_WAVE_LAYERS);
        isChanged |= ImGui::SliderFloat("Target FPS", &settings.targetFps, 5.0f, 144.0f, "%.0f");
    }

    if (ImGui::CollapsingHeader("Layers")) {
        for (int i = 0; i < settings.layerCount; ++i) {
            WaveLayerSettings& layer = settings.layers[i];
            ImGui::PushID(i);
            if (ImGui::TreeNode("Layer", "Layer %d", i)) {
                isChanged |= ImGui::SliderFloat("Position", &layer.baseY, -1.5f, 1.5f);
                isChanged |= ImGui::SliderFloat("Tilt", &layer.tilt, -1.0f, 1.0f);
                isChanged |= ImGui::SliderFloat("Amplitude", &layer.amplitude, 0.0f, 1.0f);
                isChanged |= ImGui::SliderFloat("Frequency", &layer.frequency, 0.1f, 4.0f);
                isChanged |= ImGui::SliderFloat("Thickness", &layer.thickness, 0.01f, 0.5f);
                isChanged |= ImGui::SliderFloat("Speed", &layer.speed, 0.0f, 1.0f);
                isChanged |= ImGui::Checkbox("Reverse direction", &layer.isReversed);
                isChanged |= ImGui::Checkbox("Fills below", &layer.fillsBelow);
                isChanged |= ImGui::SliderFloat("Opacity", &layer.opacity, 0.0f, 1.0f);
                isChanged |= ImGui::SliderFloat("Accent", &layer.accent, 0.0f, 1.0f);
                isChanged |= ImGui::SliderFloat("Accent speed", &layer.accentSpeed, 0.0f, 1.0f);
                isChanged |= ImGui::SliderFloat("Accent spread", &layer.accentSpread, 0.1f, 5.0f);
                isChanged |= ImGui::SliderFloat("Accent sharpness", &layer.accentSharpness, 0.5f, 8.0f);
                isChanged |= ImGui::SliderFloat("Accent offset", &layer.accentOffset, -3.1416f, 3.1416f);
                isChanged |= ImGui::SliderFloat("Seed", &layer.seed, 0.0f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
    return isChanged;
}
