#pragma once

#include "wave_background.h"

constexpr const char* WAVE_BACKGROUND_CONFIG_PATH = "background.cfg";

bool save_wave_background_settings(const WaveBackgroundSettings& settings, const char* path);

// Keys missing from the file keep their current values and unknown keys are ignored,
// so config files written before a parameter existed still load.
bool load_wave_background_settings(WaveBackgroundSettings& settings, const char* path);
