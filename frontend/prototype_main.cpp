#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#include <SDL_image.h>

#ifdef _WIN32
    #include <SDL_opengl.h>
#else
    #include <GLES3/gl3.h>
#endif

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#define WINDOW_LENGTH 1280
#define WINDOW_WIDTH 720

using json = nlohmann::json;

const std::string API_URL = "http://127.0.0.1:8000";

struct ApiResponse {
    long statusCode = 0;
    std::string body;
    std::string error;
    bool success = false;
};

static size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userData
) {
    const size_t totalSize = size * nmemb;
    auto* output = static_cast<std::string*>(userData);
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static ApiResponse apiRequest(
    const std::string& endpoint,
    bool post = false
) {
    ApiResponse response;
    CURL* curl = curl_easy_init();

    if (!curl) {
        response.error = "Could not initialize CURL.";
        return response;
    }

    const std::string url = API_URL + endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1500L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    }

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        response.error = curl_easy_strerror(result);
    } else {
        response.success = true;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
    }

    curl_easy_cleanup(curl);
    return response;
}

static bool parseApiResponse(
    const ApiResponse& response,
    json& output,
    std::string& error
) {
    if (!response.success) {
        error = "API connection failed: " + response.error;
        return false;
    }

    try {
        output = json::parse(response.body);
    } catch (const json::exception& exception) {
        error = std::string("Invalid API response: ") + exception.what();
        return false;
    }

    if (response.statusCode < 200 || response.statusCode >= 300) {
        if (output.contains("detail") && output["detail"].is_string()) {
            error = output["detail"].get<std::string>();
        } else {
            error = "API returned HTTP " + std::to_string(response.statusCode);
        }
        return false;
    }

    return true;
}

static bool checkCartridge(
    std::string& status,
    std::string& cartridgeStatus,
    std::string& gameTitle,
    std::string& platform
) {
    const ApiResponse response = apiRequest("/api/cartridge/status");
    json data;
    std::string error;

    if (!parseApiResponse(response, data, error)) {
        status = error;
        cartridgeStatus = "api_offline";
        gameTitle.clear();
        platform.clear();
        return false;
    }

    cartridgeStatus = data.value("status", std::string("unknown"));
    gameTitle.clear();
    platform.clear();

    if (data.contains("game") && data["game"].is_object()) {
        const json& game = data["game"];
        gameTitle = game.value("title", std::string("Unknown Game"));
        platform = game.value("platform", std::string("Unknown"));
    }

    if (cartridgeStatus == "ready") {
        status = "Game detected.";
    } else if (cartridgeStatus == "invalid_cartridge") {
        status = "The inserted USB does not contain a supported game.";
    } else if (cartridgeStatus == "no_cartridge") {
        status = "Insert a game USB drive to get started.";
    } else {
        status = "Cartridge status: " + cartridgeStatus;
    }

    return true;
}

static bool downloadCover(GLuint& texture, int& imageWidth, int& imageHeight,
                          std::string& error) {
    const ApiResponse response = apiRequest("/api/cartridge/cover");

    if (!response.success) {
        error = "Could not connect to the cover endpoint: " + response.error;
        return false;
    }

    if (response.statusCode < 200 || response.statusCode >= 300) {
        error = "Cover endpoint returned HTTP " + std::to_string(response.statusCode);
        return false;
    }

    if (response.body.empty()) {
        error = "The cover image response was empty.";
        return false;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(
        response.body.data(),
        static_cast<int>(response.body.size())
    );

    if (!rw) {
        error = std::string("Could not read cover image: ") + SDL_GetError();
        return false;
    }

    SDL_Surface* loaded = IMG_Load_RW(rw, 1);
    if (!loaded) {
        error = std::string("Could not decode cover image: ") + IMG_GetError();
        return false;
    }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);

    if (!rgba) {
        error = std::string("Could not convert cover image: ") + SDL_GetError();
        return false;
    }

    GLuint newTexture = 0;
    glGenTextures(1, &newTexture);
    glBindTexture(GL_TEXTURE_2D, newTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rgba->pitch / 4);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        rgba->w,
        rgba->h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba->pixels
    );

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        glDeleteTextures(1, &newTexture);
        SDL_FreeSurface(rgba);
        error = "OpenGL could not create the cover texture (error " +
                std::to_string(glError) + ").";
        return false;
    }

    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }

    texture = newTexture;
    imageWidth = rgba->w;
    imageHeight = rgba->h;
    SDL_FreeSurface(rgba);
    error.clear();
    return true;
}

static void launchGame(std::string& status, bool& gameRunning) {
    const ApiResponse response = apiRequest("/api/cartridge/launch", true);
    json data;
    std::string error;

    if (!parseApiResponse(response, data, error)) {
        status = "Launch failed: " + error;
        return;
    }

    status = data.value("message", std::string("Launch request successful."));
    if (data.contains("process") && data["process"].is_object()) {
        gameRunning = data["process"].value("running", false);
    }
}

static void checkLaunchStatus(bool& gameRunning) {
    const ApiResponse response = apiRequest("/api/launch/status");
    json data;
    std::string error;

    if (parseApiResponse(response, data, error)) {
        gameRunning = data.value("running", false);
    }
}

int main(int argc, char** argv) {
    const bool windowed = (
        argc > 1 && std::strcmp(argv[1], "--windowed") == 0
    );

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::fprintf(stderr, "CURL initialization failed.\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        curl_global_cleanup();
        return 1;
    }

    const int imageFlags = IMG_INIT_PNG;
    if ((IMG_Init(imageFlags) & imageFlags) != imageFlags) {
        std::fprintf(stderr, "SDL_image PNG initialization failed: %s\n", IMG_GetError());
        SDL_Quit();
        curl_global_cleanup();
        return 1;
    }

#ifdef _WIN32
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

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
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        curl_global_cleanup();
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "GL context failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        curl_global_cleanup();
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard |
                      ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    ImFontConfig fontCfg;
    fontCfg.SizePixels = 28.0f;
    io.Fonts->AddFontDefault(&fontCfg);

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(2.0f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl);
#ifdef _WIN32
    ImGui_ImplOpenGL3_Init("#version 130");
#else
    ImGui_ImplOpenGL3_Init("#version 300 es");
#endif

    std::string status = "Connecting to EmuStation API...";
    std::string cartridgeStatus = "checking";
    std::string gameTitle;
    std::string platform;
    bool gameRunning = false;
    bool running = true;

    GLuint coverTexture = 0;
    int coverWidth = 0;
    int coverHeight = 0;
    std::string coverError;
    std::string loadedCoverForTitle;

    checkCartridge(status, cartridgeStatus, gameTitle, platform);
    if (cartridgeStatus == "ready") {
        if (downloadCover(coverTexture, coverWidth, coverHeight, coverError)) {
            loadedCoverForTitle = gameTitle;
        }
    }

    Uint32 lastStatusCheck = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        const Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastStatusCheck >= 1500) {
            checkCartridge(status, cartridgeStatus, gameTitle, platform);

            if (cartridgeStatus == "ready" && gameTitle != loadedCoverForTitle) {
                if (downloadCover(coverTexture, coverWidth, coverHeight, coverError)) {
                    loadedCoverForTitle = gameTitle;
                }
            } else if (cartridgeStatus != "ready") {
                if (coverTexture != 0) {
                    glDeleteTextures(1, &coverTexture);
                    coverTexture = 0;
                }
                coverWidth = 0;
                coverHeight = 0;
                loadedCoverForTitle.clear();
                coverError.clear();
            }

            checkLaunchStatus(gameRunning);
            lastStatusCheck = currentTime;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGui::Begin(
            "Home",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground
        );

        const float panelWidth = 560.0f;
        const float panelHeight = 600.0f;
        ImGui::SetCursorPos(ImVec2(
            (viewport->WorkSize.x - panelWidth) * 0.5f,
            (viewport->WorkSize.y - panelHeight) * 0.5f
        ));

        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(panelWidth);
        ImGui::Text("EmuStation");
        ImGui::Separator();
        ImGui::Spacing();

        if (cartridgeStatus == "ready") {
            ImGui::TextWrapped("%s", gameTitle.c_str());
            ImGui::TextDisabled("%s", platform.c_str());
            ImGui::Spacing();

            if (coverTexture != 0 && coverWidth > 0 && coverHeight > 0) {
                const float maxImageWidth = 420.0f;
                const float maxImageHeight = 340.0f;
                const float scale = std::min(
                    maxImageWidth / static_cast<float>(coverWidth),
                    maxImageHeight / static_cast<float>(coverHeight)
                );
                const ImVec2 imageSize(
                    coverWidth * scale,
                    coverHeight * scale
                );
                const float imageX = (panelWidth - imageSize.x) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + imageX);
                ImGui::Image(
                    (ImTextureID)(intptr_t)coverTexture,
                    imageSize,
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );
            } else {
                ImGui::TextWrapped(
                    "%s",
                    coverError.empty() ? "Loading game artwork..." : coverError.c_str()
                );
            }

            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 30.0f);
            const bool canLaunch = !gameRunning;
            if (!canLaunch) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Play Game", ImVec2(500.0f, 75.0f))) {
                launchGame(status, gameRunning);
            }

            if (!canLaunch) {
                ImGui::EndDisabled();
            }

            if (gameRunning) {
                ImGui::TextDisabled("Game is running. Press the controller Home button to return.");
            }
        } else {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", status.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Insert a USB game drive to continue.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::EndGroup();
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    if (coverTexture != 0) {
        glDeleteTextures(1, &coverTexture);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    curl_global_cleanup();

    return 0;
}
