
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>

#ifdef _WIN32
    #include <SDL_opengl.h>
#else
    #include <GLES3/gl3.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>

#define WINDOW_LENGTH 1280
#define WINDOW_WIDTH 720

#ifndef _WIN32
static pid_t launch_rpcs3();
#endif

int main(int argc, char** argv) {

    // Windowed option to help with debugging
    bool windowed = (argc > 1 &&
                     std::strcmp(argv[1], "--windowed") == 0);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO |
                 SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_EVENTS) != 0) {

        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Configure OpenGL context
#ifdef _WIN32
    // Windows: Desktop OpenGL
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY
    );

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

#else
    // Raspberry Pi: OpenGL ES
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_ES
    );

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Create window
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_Window* window = SDL_CreateWindow(
        "EmuStation Home",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_LENGTH,
        WINDOW_WIDTH,
        flags
    );

    if (!window) {
        std::fprintf(
            stderr,
            "SDL_CreateWindow failed: %s\n",
            SDL_GetError()
        );

        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext gl = SDL_GL_CreateContext(window);

    if (!gl) {
        std::fprintf(
            stderr,
            "GL context failed: %s\n",
            SDL_GetError()
        );

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard |
                      ImGuiConfigFlags_NavEnableGamepad;

    io.IniFilename = nullptr;

    // Font configuration
    ImFontConfig fontCfg;
    fontCfg.SizePixels = 28.0f;

    io.Fonts->AddFontDefault(&fontCfg);

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(2.0f);

    // Initialize ImGui backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl);

#ifdef _WIN32
    ImGui_ImplOpenGL3_Init("#version 130");
#else
    ImGui_ImplOpenGL3_Init("#version 300 es");
#endif

#ifndef _WIN32
    // Only used on Linux for the current RPCS3 prototype
    pid_t child = -1;
#endif

    std::string status;

    bool running = true;

    // Main application loop
    while (running) {

#ifndef _WIN32
        // Check whether RPCS3 is still running
        if (child > 0) {

            int exitStatus = 0;

            pid_t waitResult = waitpid(
                child,
                &exitStatus,
                WNOHANG
            );

            if (waitResult == child || waitResult == -1) {

                child = -1;

                if (WIFEXITED(exitStatus) &&
                    WEXITSTATUS(exitStatus) == 127) {

                    status = "Could not start RPCS3. Check RPCS3_PATH.";

                } else {
                    status = "";
                }

            } else {

                SDL_Event waitEvent;

                if (SDL_WaitEventTimeout(&waitEvent, 250)) {

                    do {
                        if (waitEvent.type == SDL_QUIT) {
                            running = false;
                        }

                    } while (SDL_PollEvent(&waitEvent));
                }

                continue;
            }
        }
#endif

        // Process events
        SDL_Event event;

        while (SDL_PollEvent(&event)) {

            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Main window
        const ImGuiViewport* vp = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGui::Begin(
            "Home",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground
        );

        // Center button
        const ImVec2 btn(420.0f, 90.0f);

        ImGui::SetCursorPos(
            ImVec2(
                (vp->WorkSize.x - btn.x) * 0.5f,
                (vp->WorkSize.y - btn.y) * 0.5f
            )
        );

        if (ImGui::Button("Verify Game File", btn)) {

#ifdef _WIN32
            // Windows test mode: verify the fake game file exists
            std::filesystem::path gameFile =
                "../api/test_cartridge/games/crash-bandicoot.chd";

            if (std::filesystem::exists(gameFile)) {
                status = "Game file found successfully!";
            } else {
                status = "Game file not found. Check cartridge path.";
            }

#else
            // Raspberry Pi: launch RPCS3
            child = launch_rpcs3();

            if (child < 0) {
                status = "Could not launch RPCS3.";
            }
#endif
        }

        ImGui::SetItemDefaultFocus();

        // Display status message
        if (!status.empty()) {

            float w = ImGui::CalcTextSize(status.c_str()).x;

            ImGui::SetCursorPosX(
                (vp->WorkSize.x - w) * 0.5f
            );

            ImGui::TextUnformatted(status.c_str());
        }

        ImGui::End();

        // Render
        ImGui::Render();

        glViewport(
            0,
            0,
            (int)io.DisplaySize.x,
            (int)io.DisplaySize.y
        );

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        SDL_GL_SwapWindow(window);
    }

    // Application shutdown
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

#ifndef _WIN32

// Linux-only RPCS3 launcher
static pid_t launch_rpcs3() {

    pid_t pid = fork();

    if (pid == 0) {

        const char* path = std::getenv("RPCS3_PATH");

        if (!path || !path[0]) {
            _exit(127);
        }

        execlp(path, path, (char*)nullptr);

        _exit(127);
    }

    return pid;
}

#endif
