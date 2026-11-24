# Frontend Docs
## Summary
The frontend service is downloaded by the OS to launch an ImGUI launcher that acts as a console home screen that can do the following: 
- Display and load differing games from differing titles using their respective emulator (PS3 Emulation main priority for MVP).
- Allow for customizability of certain aspects of UI.
- Hold differing accounts of users and save files for specified games (Beyond MVP).

This is all possible via communication with our API service and rendering properly using our UI Tech Stack

## Tech Stack
- C++ 17 built via g++ (Language)
- Dear ImGUI v1.91.5 (UI)
- Vulkan backend launched via ImGUI (RPCS3 Rendering)

## Running Frontend
In our testing/developing phase we have two differing way in which we can test the UI on a local machine

### Windows:
```
// Dependencies (MSYS2)
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_image mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-nlohmann-json

// Run (from Git Bash, PowerShell or cmd; use mingw32-make, not make)
cd frontend
mingw32-make -f Makefile.windows run
```

### Linux
```
// Dependencies
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-image-dev libcurl4-openssl-dev nlohmann-json3-dev libgles2-mesa-dev

// Run
cd frontend
make 
```

## Animated Background
The home screen draws a procedural wave background (`background/animated_background/`) entirely in a GLSL fragment shader (`background/animated_background/shaders/waves.frag.glsl`). It renders into an offscreen texture at `resolution_scale` × the window size, at no more than `target_fps`, and is upscaled to the window every frame. While animation is disabled, the scene is drawn once and then only copied.

### Customizing
- Press **F1** to open the Background Settings panel. Every color, animation, lighting, rendering and per-layer value updates live.
- **Save** writes `background.cfg` (plain `key = value` lines) next to the executable. It is loaded automatically at startup, and missing keys keep their defaults.

### Adding a parameter
1. Add the field to `WaveBackgroundSettings` / `WaveLayerSettings` (`wave_background.h`).
2. Pack it into a free slot of `BackgroundParamsStd140` in `upload_params()` (`wave_background.cpp`), and mirror it in the `BackgroundParams` block of the shader.
3. Add a widget in `wave_background_panel.cpp` and a key in `visit_settings()` (`wave_background_config.cpp`).
