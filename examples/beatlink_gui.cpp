/**
 * Beat Link C++ GUI - DJ Link Monitor
 *
 * A Dear ImGui-based monitoring application for Pioneer DJ Link networks.
 * Inspired by Beat Link Trigger.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <cmath>

// OpenGL / GLFW / ImGui
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Beat Link
#include <beatlink/BeatLink.hpp>

// Application state
struct PlayerState {
    std::string name;
    int deviceNumber = 0;
    double bpm = 0.0;
    double effectiveBpm = 0.0;
    double pitchPercent = 0.0;
    int beatWithinBar = 1;
    bool isPlaying = false;
    bool isMaster = false;
    bool isSynced = false;
    bool isOnAir = false;
    std::chrono::steady_clock::time_point lastBeatTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    float beatFlash = 0.0f;
};

class BeatLinkApp {
public:
    std::mutex stateMutex;
    std::map<int, PlayerState> players;
    std::vector<std::string> logMessages;
    bool isRunning = false;

    void addLog(const std::string& msg) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));
        logMessages.push_back(std::string(buf) + " " + msg);
        if (logMessages.size() > 100) {
            logMessages.erase(logMessages.begin());
        }
    }

    void onDeviceFound(const beatlink::DeviceAnnouncement& device) {
        std::lock_guard<std::mutex> lock(stateMutex);
        int num = device.getDeviceNumber();
        if (players.find(num) == players.end()) {
            PlayerState state;
            state.deviceNumber = num;
            state.name = device.getDeviceName();
            state.lastUpdateTime = std::chrono::steady_clock::now();
            players[num] = state;
        }
        addLog("[+] Device found: " + device.getDeviceName() + " (#" + std::to_string(num) + ")");
    }

    void onDeviceLost(const beatlink::DeviceAnnouncement& device) {
        std::lock_guard<std::mutex> lock(stateMutex);
        int num = device.getDeviceNumber();
        players.erase(num);
        addLog("[-] Device lost: " + device.getDeviceName() + " (#" + std::to_string(num) + ")");
    }

    void onBeat(const beatlink::Beat& beat) {
        std::lock_guard<std::mutex> lock(stateMutex);
        int num = beat.getDeviceNumber();
        auto& player = players[num];
        player.deviceNumber = num;
        player.name = beat.getDeviceName();
        player.bpm = beat.getBpm() / 100.0;
        player.effectiveBpm = beat.getEffectiveTempo();
        player.pitchPercent = beatlink::Util::pitchToPercentage(beat.getPitch());
        player.beatWithinBar = beat.getBeatWithinBar();
        player.lastBeatTime = std::chrono::steady_clock::now();
        player.lastUpdateTime = std::chrono::steady_clock::now();
        player.beatFlash = 1.0f;
        player.isPlaying = true;
    }
};

static BeatLinkApp app;

// Color helpers
ImVec4 ColorFromHSV(float h, float s, float v, float a = 1.0f) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, a);
}

// Draw beat indicator (4 circles for 4/4 time)
void DrawBeatIndicator(int currentBeat, float flash) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float radius = 12.0f;
    float spacing = 30.0f;

    for (int i = 1; i <= 4; ++i) {
        ImVec2 center(pos.x + (i - 1) * spacing + radius, pos.y + radius);

        ImU32 color;
        if (i == currentBeat) {
            // Active beat - bright with flash
            float brightness = 0.5f + 0.5f * flash;
            if (i == 1) {
                // Downbeat is red
                color = ImGui::ColorConvertFloat4ToU32(ImVec4(brightness, 0.2f, 0.2f, 1.0f));
            } else {
                // Other beats are green
                color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, brightness, 0.2f, 1.0f));
            }
            drawList->AddCircleFilled(center, radius, color);
        } else {
            // Inactive beat - dim
            color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            drawList->AddCircle(center, radius, color, 0, 2.0f);
        }
    }

    ImGui::Dummy(ImVec2(4 * spacing, radius * 2 + 4));
}

// Draw player panel
void DrawPlayerPanel(PlayerState& player) {
    ImGui::PushID(player.deviceNumber);

    // Calculate time since last update
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - player.lastUpdateTime).count();
    bool isStale = elapsed > 5000;

    // Decay beat flash
    float dt = 1.0f / 60.0f; // Assume 60 FPS
    player.beatFlash = std::max(0.0f, player.beatFlash - dt * 3.0f);

    // Panel styling
    ImVec4 headerColor = isStale ? ImVec4(0.3f, 0.3f, 0.3f, 1.0f) :
                         player.isMaster ? ImVec4(0.8f, 0.6f, 0.0f, 1.0f) :
                         ImVec4(0.2f, 0.4f, 0.6f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColor);

    std::string header = player.name + " (#" + std::to_string(player.deviceNumber) + ")";
    if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(3);

        ImGui::Indent(10);

        // Beat indicator
        ImGui::Text("Beat:");
        ImGui::SameLine();
        DrawBeatIndicator(player.beatWithinBar, player.beatFlash);

        // BPM display (large)
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Default font, but larger
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        ImGui::Text("%.1f BPM", player.effectiveBpm);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        // Track BPM and pitch
        ImGui::Text("Track: %.1f BPM", player.bpm);
        ImGui::SameLine(150);
        if (player.pitchPercent >= 0) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Pitch: +%.2f%%", player.pitchPercent);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Pitch: %.2f%%", player.pitchPercent);
        }

        // Status flags
        ImGui::Text("Status:");
        ImGui::SameLine();
        if (player.isPlaying) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PLAYING");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "STOPPED");
        }
        ImGui::SameLine();
        if (player.isMaster) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "MASTER");
        }
        ImGui::SameLine();
        if (player.isSynced) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "SYNC");
        }
        ImGui::SameLine();
        if (player.isOnAir) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ON AIR");
        }

        ImGui::Unindent(10);
        ImGui::Separator();
    } else {
        ImGui::PopStyleColor(3);
    }

    ImGui::PopID();
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // GL 3.2 + GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Beat Link Monitor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 1.0f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Start Beat Link
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& d) {
        app.onDeviceFound(d);
    });
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& d) {
        app.onDeviceLost(d);
    });
    beatFinder.addBeatListener([](const beatlink::Beat& b) {
        app.onBeat(b);
    });

    if (deviceFinder.start()) {
        app.addLog("DeviceFinder started on port 50000");
        app.isRunning = true;
    } else {
        app.addLog("ERROR: Failed to start DeviceFinder");
    }

    if (beatFinder.start()) {
        app.addLog("BeatFinder started on port 50001");
    } else {
        app.addLog("ERROR: Failed to start BeatFinder");
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Beat Link Monitor", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // Header
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Beat Link C++ v%s", beatlink::Version::STRING);
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        if (app.isRunning) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "CONNECTED");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "DISCONNECTED");
        }
        ImGui::Separator();

        // Players section
        ImGui::BeginChild("Players", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.6f), true);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "DJ Link Devices");
        ImGui::Separator();

        {
            std::lock_guard<std::mutex> lock(app.stateMutex);
            if (app.players.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No devices found. Waiting for DJ Link devices...");
            } else {
                for (auto& [num, player] : app.players) {
                    DrawPlayerPanel(player);
                }
            }
        }
        ImGui::EndChild();

        // Log section
        ImGui::BeginChild("Log", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Event Log");
        ImGui::Separator();
        {
            std::lock_guard<std::mutex> lock(app.stateMutex);
            for (const auto& msg : app.logMessages) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::End();

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    beatFinder.stop();
    deviceFinder.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
