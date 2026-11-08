#include "imgui.h" // API
#include "imgui_impl_sdl2.h" // Inputs
#include "imgui_impl_opengl3.h" // Draws output via opengl3
#include <SDL.h> // Handles window & OpenGL3 context
#include <GLES3/gl3.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define WINDOW_LENGTH 1280
#define WINDOW_WIDTH 720

static pid_t launch_rpcs3();

int main(int argc, char** argv){
    // Windowed option to help me debug
    bool windowed = (argc > 1 && std::strcmp(argv[1], "--windowed") == 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!windowed) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    SDL_Window* window = SDL_CreateWindow(
        "EmuStation Home",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_LENGTH,
        WINDOW_WIDTH, 
        flags
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "GL context failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    // ImGUI SetUp
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    ImFontConfig fontCfg;
    fontCfg.SizePixels = 28.0f;
    io.Fonts->AddFontDefault(&fontCfg);
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(2.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    pid_t child = -1;
    std::string status;

    bool running = true;
    while (running) {
        // While RPCS3 is running: don't render, don't feed input to ImGui.
        if (child > 0){
            int exitStatus = 0;
            pid_t waitResult = waitpid(child, &exitStatus, WNOHANG);
            if (waitResult == child || waitResult == -1) {
                child = -1;
                if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus) == 127)
                    status = "Could not start RPCS3. Check RPCS3_PATH.";
                else
                    status = "";
            } else{
                SDL_Event waitEvent;
                if (SDL_WaitEventTimeout(&waitEvent, 250)) {
                    do {
                        if (waitEvent.type == SDL_QUIT) running = false;
                    } while (SDL_PollEvent(&waitEvent));
                }
                continue;
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)){
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin(
            "Home", 
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground
        );

        const ImVec2 btn(420.0f, 90.0f);
        ImGui::SetCursorPos(ImVec2((vp->WorkSize.x - btn.x) * 0.5f, (vp->WorkSize.y - btn.y) * 0.5f));
        if (ImGui::Button("Launch RPCS3", btn)){
            child = launch_rpcs3();
            if (child < 0) status = "fork() failed.";
        }
        ImGui::SetItemDefaultFocus();

        if (!status.empty()) {
            float w = ImGui::CalcTextSize(status.c_str()).x;
            ImGui::SetCursorPosX((vp->WorkSize.x - w) * 0.5f);
            ImGui::TextUnformatted(status.c_str());
        }

        ImGui::End();
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    // Application Shutdown
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

static pid_t launch_rpcs3(){
    pid_t pid = fork();
    if (pid == 0) {
        // ** NOTE ** Need to connect path to RPCS3 when imported into repository here.
        const char* path = std::getenv("RPCS3_PATH");
        execlp(path, path, (char*)nullptr);
        _exit(127); // exec failed
    }
    return pid;
}
