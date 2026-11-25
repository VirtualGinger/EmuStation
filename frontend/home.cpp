#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "wave_background.h"
#include "wave_background_config.h"
#include "wave_background_panel.h"

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

struct Game {
    std::string id;
    std::string title;
    std::string platform;
    std::string titleId;
    std::string version;
    GLuint cover = 0;
    int coverWidth = 0;
    int coverHeight = 0;
};

// The default ImGui font only has printable ASCII glyphs; anything else renders as '?'.
static std::string stripUnsupportedGlyphs(const std::string& text) {
    std::string printable;
    for (const char character : text) {
        if (character >= 0x20 && character <= 0x7E) {
            printable += character;
        }
    }
    return printable;
}

static std::string escapeUrlSegment(const std::string& text) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return text;
    }

    char* escaped = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
    const std::string segment = escaped ? escaped : text;
    curl_free(escaped);
    curl_easy_cleanup(curl);
    return segment;
}

static bool downloadCover(Game& game, std::string& error) {
    const ApiResponse response = apiRequest(
        "/api/cartridge/games/" + escapeUrlSegment(game.id) + "/cover"
    );

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

    game.cover = newTexture;
    game.coverWidth = rgba->w;
    game.coverHeight = rgba->h;
    SDL_FreeSurface(rgba);
    error.clear();
    return true;
}

// Returns true when the set or order of games changed.
static bool refreshGames(std::string& status, std::vector<Game>& games) {
    const ApiResponse response = apiRequest("/api/cartridge/status");
    json data;
    std::string error;
    std::vector<Game> detectedGames;

    if (!parseApiResponse(response, data, error)) {
        status = error;
    } else {
        if (data.contains("games") && data["games"].is_array()) {
            for (const json& entry : data["games"]) {
                if (!entry.is_object()) {
                    continue;
                }

                Game game;
                game.id = entry.value("id", std::string());
                game.title = stripUnsupportedGlyphs(entry.value("title", std::string("Unknown Game")));
                game.platform = stripUnsupportedGlyphs(entry.value("platform", std::string()));
                game.titleId = stripUnsupportedGlyphs(entry.value("title_id", std::string()));
                game.version = stripUnsupportedGlyphs(entry.value("version", std::string()));
                detectedGames.push_back(std::move(game));
            }
        }

        const std::string cartridgeStatus = data.value("status", std::string("unknown"));
        if (cartridgeStatus == "ready") {
            status = "";
        } else if (cartridgeStatus == "invalid_cartridge") {
            status = "The inserted USB does not contain a supported game.";
        } else if (cartridgeStatus == "no_cartridge") {
            status = "Insert a game USB drive to get started.";
        } else {
            status = "Cartridge status: " + cartridgeStatus;
        }
    }

    for (Game& game : detectedGames) {
        auto previous = std::find_if(games.begin(), games.end(), [&](const Game& known) {
            return known.id == game.id;
        });

        if (previous != games.end()) {
            game.cover = previous->cover;
            game.coverWidth = previous->coverWidth;
            game.coverHeight = previous->coverHeight;
            previous->cover = 0;
            continue;
        }

        std::string coverError;
        if (!downloadCover(game, coverError)) {
            std::fprintf(stderr, "Cover for %s unavailable: %s\n", game.id.c_str(), coverError.c_str());
        }
    }

    for (const Game& removed : games) {
        if (removed.cover != 0) {
            glDeleteTextures(1, &removed.cover);
        }
    }

    std::sort(detectedGames.begin(), detectedGames.end(), [](const Game& a, const Game& b) {
        const int order = SDL_strcasecmp(a.title.c_str(), b.title.c_str());
        return order != 0 ? order < 0 : a.id < b.id;
    });

    const bool hasChanged = !std::equal(
        games.begin(), games.end(),
        detectedGames.begin(), detectedGames.end(),
        [](const Game& a, const Game& b) { return a.id == b.id; }
    );

    games = std::move(detectedGames);
    return hasChanged;
}

static void drawBanner(ImDrawList* drawList, ImVec2 min, ImVec2 max, const char* text) {
    drawList->AddRectFilled(min, max, IM_COL32(70, 76, 96, 220), 6.0f);
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(
        ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f),
        IM_COL32(230, 232, 240, 255),
        text
    );
}

static void launchGame(const std::string& gameId, std::string& status, bool& gameRunning) {
    const ApiResponse response = apiRequest(
        "/api/cartridge/launch/" + escapeUrlSegment(gameId),
        true
    );
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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

    if (!load_gl_functions()) {
        SDL_GL_DeleteContext(gl);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        curl_global_cleanup();
        return 1;
    }

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
    bool gameRunning = false;
    bool running = true;

    std::vector<Game> games;
    bool shouldFocusFirstCard = refreshGames(status, games);
    int focusedCard = -1;
    float scrollTarget = 0.0f;
    float scrollPosition = 0.0f;

    const ImU32 cardFill = IM_COL32(18, 22, 34, 230);
    const ImU32 cardBorder = IM_COL32(255, 255, 255, 40);
    const ImU32 titleColor = IM_COL32(240, 242, 248, 255);
    const ImU32 detailColor = IM_COL32(160, 166, 184, 255);

    WaveBackground background;
    load_wave_background_settings(background.settings, WAVE_BACKGROUND_CONFIG_PATH);
    background.init();
    bool isBackgroundPanelOpen = false;
    Uint64 lastFrameCounter = SDL_GetPerformanceCounter();

    Uint32 lastStatusCheck = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
	    }
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_F1 &&
                !event.key.repeat) {
                isBackgroundPanelOpen = !isBackgroundPanelOpen;
            }
        }

        const Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastStatusCheck >= 1500) {
            if (refreshGames(status, games)) {
                shouldFocusFirstCard = true;
                focusedCard = -1;
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

        const ImVec2 screen = viewport->WorkSize;
        const int visibleCardCount = 3;
        const float gapRatio = 0.08f;
        const float cardAspect = 0.72f;
        // Widest cards that fit visibleCardCount across with a gap on every side, capped by height.
        const float cardHeight = std::min(
            screen.x / (visibleCardCount + (visibleCardCount + 1) * gapRatio) / cardAspect,
            screen.y * 0.7f
        );
        const float cardWidth = cardHeight * cardAspect;
        const float cardGap = cardWidth * gapRatio;
        const float cardPadding = cardWidth * 0.06f;
        const float cardRounding = cardWidth * 0.05f;
        // Room above and below the cards so the highlight border is not clipped by the row.
        const float borderMargin = 4.0f;
        const float rowTop = (screen.y - cardHeight) * 0.5f;
        const float lineHeight = ImGui::GetTextLineHeight();
        const float textWidth = cardWidth - cardPadding * 2.0f;

        // Drawn at a fixed screen position so it never moves with window or row scrolling.
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(viewport->WorkPos.x + cardGap, viewport->WorkPos.y + cardGap),
            titleColor,
            "EmuStation"
        );

        if (games.empty()) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 cardMin(
                viewport->WorkPos.x + (screen.x - cardWidth) * 0.5f,
                viewport->WorkPos.y + rowTop
            );
            const ImVec2 cardMax(cardMin.x + cardWidth, cardMin.y + cardHeight);
            drawList->AddRectFilled(cardMin, cardMax, cardFill, cardRounding);
            drawList->AddRect(cardMin, cardMax, cardBorder, cardRounding, 0, 1.5f);

            const char* message = "No Game Detected in EmuStation";
            const ImVec2 messageSize = ImGui::CalcTextSize(message, nullptr, false, textWidth);
            drawList->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize(),
                ImVec2(
                    cardMin.x + (cardWidth - messageSize.x) * 0.5f,
                    cardMin.y + (cardHeight - messageSize.y) * 0.5f
                ),
                titleColor,
                message,
                nullptr,
                textWidth
            );

            const ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
            ImGui::SetCursorPos(ImVec2(
                std::max(cardGap, (screen.x - statusSize.x) * 0.5f),
                rowTop + cardHeight + cardGap
            ));
            ImGui::PushTextWrapPos(screen.x - cardGap);
            ImGui::TextDisabled("%s", status.c_str());
            ImGui::PopTextWrapPos();
        } else {
            const float cardStep = cardWidth + cardGap;
            const int gameCount = static_cast<int>(games.size());
            const float rowWidth = gameCount * cardStep - cardGap;
            const float visibleWidth = std::min(gameCount, visibleCardCount) * cardStep - cardGap;
            const float rowPadding = (screen.x - visibleWidth) * 0.5f;
            const float maxScroll = std::max(0, gameCount - visibleCardCount) * cardStep;
            const float* accent = background.settings.accentPrimary;
            const ImU32 highlightBorder = ImGui::ColorConvertFloat4ToU32(
                ImVec4(accent[0], accent[1], accent[2], 1.0f)
            );

            ImGui::SetCursorPos(ImVec2(0.0f, rowTop - borderMargin));
            ImGui::SetNextWindowContentSize(ImVec2(rowWidth + rowPadding * 2.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::BeginChild(
                "GameRow",
                ImVec2(screen.x, cardHeight + borderMargin * 2.0f),
                ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBackground
            );

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                scrollTarget -= (io.MouseWheel + io.MouseWheelH) * cardStep;
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    scrollTarget -= io.MouseDelta.x;
                    scrollPosition = scrollTarget;
                }
            }

            const float dragThreshold = io.MouseDragThreshold;
            const bool wasSwipe =
                io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > dragThreshold * dragThreshold;
            // launchGame can flip gameRunning mid-loop; a snapshot keeps Begin/EndDisabled paired.
            const bool isLaunchLocked = gameRunning;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            if (isLaunchLocked) {
                ImGui::BeginDisabled();
            }

            for (size_t index = 0; index < games.size(); ++index) {
                const Game& game = games[index];
                ImGui::SetCursorPos(ImVec2(rowPadding + index * cardStep, borderMargin));

                if (index == 0 && shouldFocusFirstCard) {
                    ImGui::SetKeyboardFocusHere();
                    shouldFocusFirstCard = false;
                }

                ImGui::PushID(game.id.c_str());
                const bool isPressed = ImGui::InvisibleButton("card", ImVec2(cardWidth, cardHeight));
                ImGui::PopID();

                if (isPressed && !(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && wasSwipe)) {
                    launchGame(game.id, status, gameRunning);
                }

                if (ImGui::IsItemFocused() && focusedCard != static_cast<int>(index)) {
                    focusedCard = static_cast<int>(index);
                    // Keeps the focused card in the middle slot; the clamp below pins the row ends.
                    scrollTarget = (static_cast<float>(index) - (visibleCardCount - 1) / 2) * cardStep;
                }

                const bool isHighlighted = ImGui::IsItemFocused() || ImGui::IsItemHovered();
                const ImVec2 cardMin = ImGui::GetItemRectMin();
                const ImVec2 cardMax = ImGui::GetItemRectMax();
                drawList->AddRectFilled(cardMin, cardMax, cardFill, cardRounding);
                drawList->AddRect(
                    cardMin,
                    cardMax,
                    isHighlighted ? highlightBorder : cardBorder,
                    cardRounding,
                    0,
                    isHighlighted ? 4.0f : 1.5f
                );

                const ImVec2 coverMin(cardMin.x + cardPadding, cardMin.y + cardPadding);
                const ImVec2 coverMax(cardMax.x - cardPadding, cardMin.y + cardHeight * 0.6f);
                if (game.cover != 0 && game.coverWidth > 0 && game.coverHeight > 0) {
                    const float scale = std::min(
                        (coverMax.x - coverMin.x) / game.coverWidth,
                        (coverMax.y - coverMin.y) / game.coverHeight
                    );
                    const ImVec2 imageMin(
                        coverMin.x + (coverMax.x - coverMin.x - game.coverWidth * scale) * 0.5f,
                        coverMin.y + (coverMax.y - coverMin.y - game.coverHeight * scale) * 0.5f
                    );
                    drawList->AddImage(
                        (ImTextureID)(intptr_t)game.cover,
                        imageMin,
                        ImVec2(imageMin.x + game.coverWidth * scale, imageMin.y + game.coverHeight * scale)
                    );
                } else {
                    const float coverCenterY = (coverMin.y + coverMax.y) * 0.5f;
                    drawBanner(
                        drawList,
                        ImVec2(coverMin.x, coverCenterY - lineHeight),
                        ImVec2(coverMax.x, coverCenterY + lineHeight),
                        "Unavailable"
                    );
                }

                const float detailTop = cardMax.y - cardPadding - lineHeight;
                const ImVec4 titleClip(coverMin.x, coverMax.y, coverMax.x, detailTop);
                drawList->AddText(
                    ImGui::GetFont(),
                    ImGui::GetFontSize(),
                    ImVec2(coverMin.x, coverMax.y + cardPadding * 0.5f),
                    titleColor,
                    game.title.c_str(),
                    nullptr,
                    textWidth,
                    &titleClip
                );

                if (game.titleId.empty() && game.version.empty()) {
                    drawBanner(
                        drawList,
                        ImVec2(coverMin.x, detailTop - 4.0f),
                        ImVec2(coverMax.x, detailTop + lineHeight + 4.0f),
                        "Unavailable"
                    );
                } else {
                    std::string details = game.platform;
                    if (!game.titleId.empty()) {
                        details += (details.empty() ? "" : " | ") + game.titleId;
                    }
                    if (!game.version.empty()) {
                        details += (details.empty() ? "v" : " | v") + game.version;
                    }

                    const ImVec4 detailClip(coverMin.x, detailTop, coverMax.x, cardMax.y);
                    drawList->AddText(
                        ImGui::GetFont(),
                        ImGui::GetFontSize(),
                        ImVec2(coverMin.x, detailTop),
                        detailColor,
                        details.c_str(),
                        nullptr,
                        0.0f,
                        &detailClip
                    );
                }
            }

            if (isLaunchLocked) {
                ImGui::EndDisabled();
            }

            scrollTarget = std::clamp(scrollTarget, 0.0f, maxScroll);
            scrollPosition = std::clamp(scrollPosition, 0.0f, maxScroll);
            scrollPosition += (scrollTarget - scrollPosition) * std::min(1.0f, io.DeltaTime * 12.0f);
            ImGui::SetScrollX(scrollPosition);

            ImGui::EndChild();
            ImGui::PopStyleVar();

            const float statusTop = screen.y - cardGap - lineHeight;
            if (gameRunning) {
                ImGui::SetCursorPos(ImVec2(cardGap, statusTop - lineHeight * 1.5f));
                ImGui::TextDisabled("Game is running. Press the controller Home button to return.");
            }
            ImGui::SetCursorPos(ImVec2(cardGap, statusTop));
            ImGui::TextUnformatted(status.c_str());
        }

        ImGui::End();

        if (isBackgroundPanelOpen &&
            draw_wave_background_panel(background.settings, &isBackgroundPanelOpen)) {
            background.request_redraw();
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Clamped so a stall (e.g. a slow API request) doesn't jump the waves forward.
        Uint64 frameCounter = SDL_GetPerformanceCounter();
        double deltaSeconds = std::min(
            0.1,
            double(frameCounter - lastFrameCounter) / SDL_GetPerformanceFrequency()
        );
        lastFrameCounter = frameCounter;

        background.render(
            deltaSeconds,
            (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x),
            (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y)
        );

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    for (const Game& game : games) {
        if (game.cover != 0) {
            glDeleteTextures(1, &game.cover);
        }
    }

    background.shutdown();
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
