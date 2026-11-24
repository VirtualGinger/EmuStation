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
cd frontend
make -f Makefile.windows run
```

### Linux
```
// Dependencies
sudo apt install build-essential pkg-config /libsdl2-dev libgles2-mesa-dev

// Run
cd frontend
make 
```