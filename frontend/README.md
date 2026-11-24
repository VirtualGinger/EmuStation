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
