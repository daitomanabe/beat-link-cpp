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
#include <algorithm>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <memory>
#include <cstdio>
#include <cctype>
#include <thread>
#include <stdexcept>
#include <functional>

// OpenGL / GLFW / ImGui
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Beat Link
#include <beatlink/BeatLink.hpp>
#include <beatlink/ui/WaveformRender.hpp>

// Application state
struct PlayerState {
    std::string name;
    std::string title;
    std::string artist;
    std::string signature;
    int deviceNumber = 0;
    double bpm = 0.0;
    double effectiveBpm = 0.0;
    double pitchPercent = 0.0;
    int beatWithinBar = 1;
    bool isPlaying = false;
    bool isMaster = false;
    bool isSynced = false;
    bool isOnAir = false;
    int durationSeconds = 0;
    int64_t playbackPositionMs = -1;
    std::chrono::steady_clock::time_point lastBeatTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    float beatFlash = 0.0f;
    std::shared_ptr<beatlink::data::TrackMetadata> metadata;
    std::shared_ptr<beatlink::data::BeatGrid> beatGrid;
    std::shared_ptr<beatlink::data::WaveformPreview> waveformPreview;
    std::shared_ptr<beatlink::data::WaveformDetail> waveformDetail;
};

class BeatLinkApp {
public:
    std::mutex stateMutex;
    std::map<int, PlayerState> players;
    std::unordered_map<int, beatlink::data::TrackPositionListenerPtr> positionListeners;
    std::vector<std::string> logMessages;
    bool isRunning = false;
    std::atomic<bool> dataFindersStarted{false};

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
        const int num = device.getDeviceNumber();
        beatlink::data::TrackPositionListenerPtr newListener;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (players.find(num) == players.end()) {
                PlayerState state;
                state.deviceNumber = num;
                state.name = device.getDeviceName();
                state.lastUpdateTime = std::chrono::steady_clock::now();
                players[num] = state;
            }
            if (positionListeners.find(num) == positionListeners.end()) {
                newListener = std::make_shared<beatlink::data::TrackPositionCallbacks>(
                    [this, num](const std::shared_ptr<beatlink::data::TrackPositionUpdate>& update) {
                        onTrackPosition(num, update);
                    });
                positionListeners[num] = newListener;
            }
        }
        if (newListener) {
            beatlink::data::TimeFinder::getInstance().addTrackPositionListener(num, newListener);
        }
        addLog("[+] Device found: " + device.getDeviceName() + " (#" + std::to_string(num) + ")");
        startDataFindersAsync();
    }

    void onDeviceLost(const beatlink::DeviceAnnouncement& device) {
        const int num = device.getDeviceNumber();
        beatlink::data::TrackPositionListenerPtr listenerToRemove;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            players.erase(num);
            auto listenerIt = positionListeners.find(num);
            if (listenerIt != positionListeners.end()) {
                listenerToRemove = listenerIt->second;
                positionListeners.erase(listenerIt);
            }
        }
        if (listenerToRemove) {
            beatlink::data::TimeFinder::getInstance().removeTrackPositionListener(listenerToRemove);
        }
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

    void onDeviceUpdate(const beatlink::DeviceUpdate& update) {
        auto cdj = dynamic_cast<const beatlink::CdjStatus*>(&update);
        if (!cdj) {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[cdj->getDeviceNumber()];
        player.deviceNumber = cdj->getDeviceNumber();
        player.name = cdj->getDeviceName();
        player.isPlaying = cdj->isPlaying();
        player.isMaster = cdj->isTempoMaster();
        player.isSynced = cdj->isSynced();
        player.isOnAir = cdj->isOnAir();
        player.bpm = cdj->getBpm() / 100.0;
        player.effectiveBpm = cdj->getEffectiveTempo();
        player.pitchPercent = beatlink::Util::pitchToPercentage(cdj->getPitch());
        player.beatWithinBar = cdj->getBeatWithinBar();
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onTrackMetadata(const beatlink::data::TrackMetadataUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[update.player];
        player.deviceNumber = update.player;
        if (!update.metadata) {
            player.metadata.reset();
            player.title.clear();
            player.artist.clear();
            player.signature.clear();
            player.durationSeconds = 0;
            player.waveformPreview.reset();
            player.waveformDetail.reset();
            player.beatGrid.reset();
            player.playbackPositionMs = -1;
            return;
        }
        const bool trackChanged = player.metadata && update.metadata &&
                                  player.metadata->getTrackReference() != update.metadata->getTrackReference();
        player.metadata = update.metadata;
        player.title = update.metadata->getTitle();
        if (update.metadata->getArtist()) {
            player.artist = update.metadata->getArtist()->getLabel();
        } else {
            player.artist.clear();
        }
        player.durationSeconds = update.metadata->getDuration();
        if (trackChanged) {
            player.beatGrid.reset();
            player.waveformPreview.reset();
            player.waveformDetail.reset();
            player.signature.clear();
            player.playbackPositionMs = -1;
        }
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onBeatGrid(const beatlink::data::BeatGridUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[update.player];
        player.beatGrid = update.beatGrid;
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onWaveformPreview(const beatlink::data::WaveformPreviewUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[update.player];
        player.waveformPreview = update.preview;
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onWaveformDetail(const beatlink::data::WaveformDetailUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[update.player];
        player.waveformDetail = update.detail;
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onTrackPosition(int playerNumber, const std::shared_ptr<beatlink::data::TrackPositionUpdate>& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[playerNumber];
        if (!update) {
            player.playbackPositionMs = -1;
            player.isPlaying = false;
            return;
        }
        player.playbackPositionMs = update->milliseconds;
        player.isPlaying = update->playing;
        player.beatWithinBar = update->getBeatWithinBar();
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void onSignature(const beatlink::data::SignatureUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto& player = players[update.player];
        player.signature = update.signature;
        player.lastUpdateTime = std::chrono::steady_clock::now();
    }

    void startDataFindersAsync() {
        bool expected = false;
        if (!dataFindersStarted.compare_exchange_strong(expected, true)) {
            return;
        }

        BeatLinkApp* self = this;
        std::thread([self]() {
            auto startAndLog = [self](const std::string& label, const std::function<void()>& startFn) {
                try {
                    startFn();
                    self->addLog(label + " started");
                } catch (const std::exception& e) {
                    self->addLog("ERROR: " + label + " failed: " + std::string(e.what()));
                } catch (...) {
                    self->addLog("ERROR: " + label + " failed: unknown error");
                }
            };

            startAndLog("VirtualCdj", []() {
                if (!beatlink::VirtualCdj::getInstance().start()) {
                    throw std::runtime_error("start returned false");
                }
            });

            startAndLog("MetadataFinder", []() {
                beatlink::data::MetadataFinder::getInstance().start();
            });

            startAndLog("BeatGridFinder", []() {
                beatlink::data::BeatGridFinder::getInstance().start();
            });

            startAndLog("WaveformFinder", []() {
                beatlink::data::WaveformFinder::getInstance().start();
            });

            startAndLog("SignatureFinder", []() {
                beatlink::data::SignatureFinder::getInstance().start();
            });

            startAndLog("TimeFinder", []() {
                beatlink::data::TimeFinder::getInstance().start();
            });
        }).detach();
    }
};

static BeatLinkApp app;

// Color helpers
ImVec4 ColorFromHSV(float h, float s, float v, float a = 1.0f) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, a);
}

std::string FormatTimeMs(int64_t ms) {
    if (ms < 0) {
        return "--:--";
    }
    const int totalSeconds = static_cast<int>(ms / 1000);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
    return buf;
}

int GetBeatsPerBar(const std::string& signature) {
    int beats = 0;
    for (char ch : signature) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            beats = beats * 10 + (ch - '0');
        } else if (beats > 0) {
            break;
        }
    }
    if (beats <= 0) {
        return 4;
    }
    return std::min(std::max(beats, 1), 12);
}

// Draw beat indicator (variable number of beats per bar).
void DrawBeatIndicator(int currentBeat, int beatsPerBar, float flash) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    beatsPerBar = std::min(std::max(beatsPerBar, 1), 12);
    const float radius = (beatsPerBar > 6) ? 8.0f : 12.0f;
    const float spacing = radius * 2.4f;
    const int displayBeat = ((currentBeat - 1) % beatsPerBar) + 1;

    for (int i = 1; i <= beatsPerBar; ++i) {
        ImVec2 center(pos.x + (i - 1) * spacing + radius, pos.y + radius);

        ImU32 color;
        if (i == displayBeat) {
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

    ImGui::Dummy(ImVec2(beatsPerBar * spacing, radius * 2 + 4));
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

        const int beatsPerBar = GetBeatsPerBar(player.signature);

        // Beat indicator
        ImGui::Text("Beat:");
        ImGui::SameLine();
        DrawBeatIndicator(player.beatWithinBar, beatsPerBar, player.beatFlash);

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

        ImGui::Separator();
        ImGui::Text("Track:");
        if (!player.title.empty()) {
            ImGui::TextWrapped("%s", player.title.c_str());
        }
        if (!player.artist.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", player.artist.c_str());
        }
        if (player.title.empty() && player.artist.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No track loaded");
        }
        if (!player.signature.empty()) {
            ImGui::Text("Signature: %s", player.signature.c_str());
        }

        const int64_t durationMs = (player.durationSeconds > 0)
            ? static_cast<int64_t>(player.durationSeconds) * 1000
            : -1;
        if (durationMs > 0) {
            const float rawProgress = (player.playbackPositionMs >= 0)
                ? static_cast<float>(player.playbackPositionMs) / static_cast<float>(durationMs)
                : 0.0f;
            const float progress = std::min(std::max(rawProgress, 0.0f), 1.0f);
            const std::string timeLabel = FormatTimeMs(player.playbackPositionMs) + " / " +
                                          FormatTimeMs(durationMs);
            ImGui::ProgressBar(progress, ImVec2(-1, 0), timeLabel.c_str());
        } else if (player.playbackPositionMs >= 0) {
            ImGui::Text("Position: %s", FormatTimeMs(player.playbackPositionMs).c_str());
        }

        const beatlink::data::CueList* cues = nullptr;
        if (player.metadata && player.metadata->getCueList()) {
            cues = player.metadata->getCueList().get();
        }

        if (player.waveformPreview) {
            beatlink::ui::WaveformPreviewDrawOptions options;
            options.height = 70.0f;
            std::string previewId = "WaveformPreview##" + std::to_string(player.deviceNumber);
            beatlink::ui::DrawWaveformPreview(previewId.c_str(), *player.waveformPreview,
                                              player.metadata.get(), cues,
                                              static_cast<float>(player.playbackPositionMs),
                                              options);
        }
        if (player.waveformDetail) {
            beatlink::ui::WaveformDetailDrawOptions options;
            options.height = 120.0f;
            std::string detailId = "WaveformDetail##" + std::to_string(player.deviceNumber);
            beatlink::ui::DrawWaveformDetail(detailId.c_str(), *player.waveformDetail,
                                             player.metadata.get(), player.beatGrid.get(), cues,
                                             static_cast<float>(player.playbackPositionMs),
                                             options);
        }

        ImGui::Unindent(10);
        ImGui::Separator();
    } else {
        ImGui::PopStyleColor(3);
    }

    ImGui::PopID();
}

int main() {
    glfwSetErrorCallback([](int error, const char* description) {
        std::cerr << "GLFW error " << error << ": " << (description ? description : "(unknown)") << std::endl;
    });

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
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Beat Link Monitor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwShowWindow(window);
    glfwFocusWindow(window);
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
    auto& virtualCdj = beatlink::VirtualCdj::getInstance();
    auto& metadataFinder = beatlink::data::MetadataFinder::getInstance();
    auto& beatGridFinder = beatlink::data::BeatGridFinder::getInstance();
    auto& waveformFinder = beatlink::data::WaveformFinder::getInstance();
    auto& signatureFinder = beatlink::data::SignatureFinder::getInstance();

    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& d) {
        app.onDeviceFound(d);
    });
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& d) {
        app.onDeviceLost(d);
    });
    beatFinder.addBeatListener([](const beatlink::Beat& b) {
        app.onBeat(b);
    });
    virtualCdj.addUpdateListener(std::make_shared<beatlink::DeviceUpdateCallbacks>(
        [](const beatlink::DeviceUpdate& update) {
            app.onDeviceUpdate(update);
        }));
    metadataFinder.addTrackMetadataListener(std::make_shared<beatlink::data::TrackMetadataCallbacks>(
        [](const beatlink::data::TrackMetadataUpdate& update) {
            app.onTrackMetadata(update);
        }));
    beatGridFinder.addBeatGridListener(std::make_shared<beatlink::data::BeatGridCallbacks>(
        [](const beatlink::data::BeatGridUpdate& update) {
            app.onBeatGrid(update);
        }));
    waveformFinder.addWaveformListener(std::make_shared<beatlink::data::WaveformCallbacks>(
        [](const beatlink::data::WaveformPreviewUpdate& update) {
            app.onWaveformPreview(update);
        },
        [](const beatlink::data::WaveformDetailUpdate& update) {
            app.onWaveformDetail(update);
        }));
    signatureFinder.addSignatureListener(std::make_shared<beatlink::data::SignatureCallbacks>(
        [](const beatlink::data::SignatureUpdate& update) {
            app.onSignature(update);
        }));

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

    app.addLog("Waiting for devices to start metadata/time services");

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
    beatlink::data::TimeFinder::getInstance().stop();
    signatureFinder.stop();
    waveformFinder.stop();
    beatGridFinder.stop();
    metadataFinder.stop();
    virtualCdj.stop();
    beatFinder.stop();
    deviceFinder.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
