
# Overview
This folder holds all the background elements for specified background elements that are used in varying differing pages within our UI. Each with a description of how they work and function.

## Animated Background (Home Page)
The home screen draws a procedural wave background (`background/animated_background/`) entirely in a GLSL fragment shader (`background/animated_background/shaders/waves.frag.glsl`). It renders into an offscreen texture at `resolution_scale` × the window size, at no more than `target_fps`, and is upscaled to the window every frame. While animation is disabled, the scene is drawn once and then only copied.

### Customizing
- Logic for this will be extended next semester.

### Adding a parameter
Parameters will allow for customizability to be present for a user of EmuStation. These parameters will be necessary for the extension of customizability to come.
1. Add the field to `WaveBackgroundSettings` / `WaveLayerSettings` (`wave_background.h`).
2. Pack it into a free slot of `BackgroundParamsStd140` in `upload_params()` (`wave_background.cpp`), and mirror it in the `BackgroundParams` block of the shader.
3. Add a widget in `wave_background_panel.cpp` and a key in `visit_settings()` (`wave_background_config.cpp`).

