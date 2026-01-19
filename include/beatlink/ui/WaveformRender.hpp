#pragma once

/**
 * WaveformRender - ImGui-based waveform rendering functions
 *
 * C++ equivalent of Java's WaveformPreviewComponent and WaveformDetailComponent.
 * Uses Dear ImGui for immediate-mode rendering instead of Swing's retained mode.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"

#include "../data/BeatGrid.hpp"
#include "../data/CueList.hpp"
#include "../data/PlaybackState.hpp"
#include "../data/SongStructure.hpp"
#include "../data/TrackMetadata.hpp"
#include "../data/WaveformDetail.hpp"
#include "../data/WaveformPreview.hpp"
#include "OverlayPainter.hpp"

namespace beatlink::ui {

inline ImU32 toImU32(const beatlink::data::Color& color) {
    return IM_COL32(color.r, color.g, color.b, color.a);
}

/// Layout constants matching Java WaveformPreviewComponent
namespace WaveformLayout {
    constexpr int CUE_MARKER_TOP = 4;
    constexpr int CUE_MARKER_HEIGHT = 4;
    constexpr int POSITION_MARKER_TOP = CUE_MARKER_TOP + CUE_MARKER_HEIGHT;
    constexpr int WAVEFORM_TOP = POSITION_MARKER_TOP + 2;
    constexpr int PLAYBACK_BAR_HEIGHT = 4;
    constexpr int MINUTE_MARKER_HEIGHT = 4;
    constexpr int WAVEFORM_MARGIN = 4;
    constexpr int MIN_WAVEFORM_HEIGHT = 31;
    constexpr int MIN_WAVEFORM_WIDTH = 200;
}

struct WaveformPreviewDrawOptions {
    float width{0.0f};
    float height{0.0f};
    beatlink::data::Color backgroundColor{0, 0, 0, 255};
    beatlink::data::Color indicatorColor{255, 255, 255, 200};
    beatlink::data::Color emphasisColor{255, 64, 64, 230};
    beatlink::data::Color playedBrightColor{255, 255, 255, 75};
    beatlink::data::Color playedDimColor{255, 255, 255, 35};
    beatlink::data::Color unplayedDimColor{255, 255, 255, 170};
    bool drawCues{true};
    bool drawMinuteMarkers{true};
    bool drawPlaybackPosition{true};
    bool drawPlaybackBar{true};
    bool drawPhrases{false};
    OverlayPainter overlayPainter{nullptr};
    std::vector<beatlink::data::PlaybackState> playbackStates{};
};

/**
 * Draw a waveform preview with all decorations.
 *
 * @param id ImGui widget ID
 * @param preview The waveform preview data
 * @param metadata Track metadata (for duration)
 * @param beatGrid Beat grid (for phrase time conversion)
 * @param cues Cue list
 * @param songStructure Song structure for phrase drawing (optional)
 * @param playbackPositionMs Current playback position in milliseconds
 * @param options Drawing options
 */
inline void DrawWaveformPreview(const char* id,
                                const beatlink::data::WaveformPreview& preview,
                                const beatlink::data::TrackMetadata* metadata,
                                const beatlink::data::BeatGrid* beatGrid,
                                const beatlink::data::CueList* cues,
                                const beatlink::data::SongStructure* songStructure,
                                float playbackPositionMs,
                                const WaveformPreviewDrawOptions& options = {}) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float width = (options.width > 0.0f) ? options.width : avail.x;
    const float height = (options.height > 0.0f) ? options.height : std::max(64.0f, avail.y);
    ImVec2 size(width, height);

    ImGui::InvisibleButton(id, size);

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), toImU32(options.backgroundColor));

    const int segmentCount = preview.getSegmentCount();
    if (segmentCount <= 0 || preview.getMaxHeight() <= 0) {
        return;
    }

    const float maxHeight = static_cast<float>(preview.getMaxHeight());
    const float segmentPerPixel = static_cast<float>(segmentCount) / width;
    const float waveformBottom = pos.y + height;

    for (int x = 0; x < static_cast<int>(width); ++x) {
        const int segment = static_cast<int>(x * segmentPerPixel);
        if (segment >= segmentCount) {
            continue;
        }

        if (preview.getStyle() == beatlink::data::WaveformStyle::THREE_BAND) {
            float low = preview.segmentHeight(segment, beatlink::data::ThreeBandLayer::LOW) / maxHeight;
            float mid = preview.segmentHeight(segment, beatlink::data::ThreeBandLayer::MID) / maxHeight;
            float high = preview.segmentHeight(segment, beatlink::data::ThreeBandLayer::HIGH) / maxHeight;

            float lowHeight = low * height;
            float midHeight = mid * height;
            float highHeight = high * height;

            ImVec2 x0(pos.x + x, waveformBottom);
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - lowHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::LOW)));
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - midHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::MID)));
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - highHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::HIGH)));
        } else {
            const int backHeightValue = preview.segmentHeight(segment, false);
            const float backHeight = (backHeightValue / maxHeight) * height;
            const int frontHeightValue = preview.segmentHeight(segment, true);
            const float frontHeight = (frontHeightValue / maxHeight) * height;
            ImVec2 x0(pos.x + x, waveformBottom);
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - backHeight),
                              toImU32(preview.segmentColor(segment, false)));
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - frontHeight),
                              toImU32(preview.segmentColor(segment, true)));
        }
    }

    const float totalMs = metadata ? static_cast<float>(metadata->getDuration() * 1000) : 0.0f;
    if (options.drawCues && cues && totalMs > 0.0f) {
        for (const auto& entry : cues->getEntries()) {
            const float position = static_cast<float>(entry.cueTime) / totalMs;
            const float x = pos.x + position * width;
            drawList->AddRectFilled(ImVec2(x - 1.0f, pos.y), ImVec2(x + 1.0f, pos.y + 6.0f),
                                    toImU32(options.indicatorColor));
            if (entry.isLoop) {
                const float loopPos = static_cast<float>(entry.loopTime) / totalMs;
                const float lx = pos.x + loopPos * width;
                drawList->AddRectFilled(ImVec2(lx - 1.0f, pos.y), ImVec2(lx + 1.0f, pos.y + 6.0f),
                                        toImU32(options.indicatorColor));
            }
        }
    }

    if (options.drawMinuteMarkers && totalMs > 0.0f) {
        const int minutes = static_cast<int>(totalMs / 60000.0f);
        for (int minute = 1; minute <= minutes; ++minute) {
            const float position = static_cast<float>(minute * 60000.0f) / totalMs;
            const float x = pos.x + position * width;
            drawList->AddLine(ImVec2(x, pos.y + height - 6.0f), ImVec2(x, pos.y + height),
                              toImU32(options.indicatorColor));
        }
    }

    // Draw song structure (phrases) at the bottom
    if (options.drawPhrases && songStructure && songStructure->isValid() && beatGrid && totalMs > 0.0f) {
        const float phraseBarHeight = static_cast<float>(WaveformLayout::MINUTE_MARKER_HEIGHT);
        const float phraseBarTop = pos.y + height - phraseBarHeight - 1.0f;

        for (size_t i = 0; i < songStructure->getEntryCount(); ++i) {
            const auto& entry = songStructure->getEntry(i);
            const int endBeat = (i == songStructure->getEntryCount() - 1) ?
                songStructure->getEndBeat() :
                songStructure->getEntry(i + 1).beat;

            // Convert beat numbers to X coordinates
            const float startTime = static_cast<float>(beatGrid->getTimeWithinTrack(entry.beat));
            const float endTime = static_cast<float>(beatGrid->getTimeWithinTrack(endBeat));
            const float x1 = pos.x + (startTime / totalMs) * width;
            const float x2 = pos.x + (endTime / totalMs) * width - 1.0f;

            if (x2 > pos.x && x1 < pos.x + width) {
                const auto phraseColor = entry.getColor(songStructure->getMood());
                drawList->AddRectFilled(
                    ImVec2(x1, phraseBarTop),
                    ImVec2(x2, phraseBarTop + phraseBarHeight - 1.0f),
                    toImU32(phraseColor));
            }
        }
    }

    // Draw playback progress bar (played portion dimmed)
    if (options.drawPlaybackBar && totalMs > 0.0f && playbackPositionMs >= 0.0f) {
        const float position = playbackPositionMs / totalMs;
        const float playedX = pos.x + position * width;

        // Draw played portion with dim overlay
        drawList->AddRectFilled(
            ImVec2(pos.x, pos.y),
            ImVec2(playedX, pos.y + height),
            toImU32(options.playedDimColor));
    }

    // Draw multiple player positions
    if (!options.playbackStates.empty() && totalMs > 0.0f) {
        for (const auto& state : options.playbackStates) {
            if (state.position >= 0) {
                const float statePosition = static_cast<float>(state.position) / totalMs;
                const float x = pos.x + statePosition * width;

                // Draw position marker with player color
                const auto markerColor = state.playing ?
                    beatlink::data::Color{255, 255, 255, 200} :
                    beatlink::data::Color{128, 128, 128, 150};

                drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height),
                                  toImU32(markerColor));

                // Draw small player number indicator at top
                if (state.player > 0) {
                    char label[4];
                    snprintf(label, sizeof(label), "%d", state.player);
                    drawList->AddText(ImVec2(x + 2.0f, pos.y + 2.0f), toImU32(markerColor), label);
                }
            }
        }
    }

    // Draw main playback position (always on top)
    if (options.drawPlaybackPosition && totalMs > 0.0f && playbackPositionMs >= 0.0f) {
        const float position = playbackPositionMs / totalMs;
        const float x = pos.x + position * width;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height),
                          toImU32(options.emphasisColor));
    }

    // Call overlay painter if provided
    if (options.overlayPainter) {
        OverlayContext ctx;
        ctx.drawList = drawList;
        ctx.position = pos;
        ctx.size = size;
        ctx.durationMs = totalMs;
        ctx.playbackPositionMs = playbackPositionMs;
        ctx.waveformWidth = width;
        ctx.waveformMargin = 0.0f;
        options.overlayPainter(ctx);
    }
}

/**
 * Backward-compatible overload without beatGrid and songStructure parameters.
 */
inline void DrawWaveformPreview(const char* id,
                                const beatlink::data::WaveformPreview& preview,
                                const beatlink::data::TrackMetadata* metadata,
                                const beatlink::data::CueList* cues,
                                float playbackPositionMs,
                                const WaveformPreviewDrawOptions& options = {}) {
    DrawWaveformPreview(id, preview, metadata, nullptr, cues, nullptr, playbackPositionMs, options);
}

/**
 * Get tooltip text for a position within the waveform preview.
 *
 * @param mousePos The mouse position in screen coordinates
 * @param componentPos The top-left position of the waveform component
 * @param componentSize The size of the waveform component
 * @param metadata Track metadata (for duration)
 * @param cues Cue list (for cue descriptions)
 * @return Tooltip text, or empty string if nothing to display
 */
inline std::string GetWaveformPreviewTooltip(
    const ImVec2& mousePos,
    const ImVec2& componentPos,
    const ImVec2& componentSize,
    const beatlink::data::TrackMetadata* metadata,
    const beatlink::data::CueList* cues)
{
    if (!metadata || metadata->getDuration() <= 0) {
        return "";
    }

    const float totalMs = static_cast<float>(metadata->getDuration() * 1000);
    const float relativeX = mousePos.x - componentPos.x;
    const float ratio = relativeX / componentSize.x;

    if (ratio < 0.0f || ratio > 1.0f) {
        return "";
    }

    // Check for cue points
    if (cues) {
        for (const auto& entry : cues->getEntries()) {
            const float cueRatio = static_cast<float>(entry.cueTime) / totalMs;
            const float cueX = componentPos.x + cueRatio * componentSize.x;
            if (std::abs(mousePos.x - cueX) < 5.0f && mousePos.y < componentPos.y + 12.0f) {
                return entry.comment.empty() ?
                    ("Cue " + std::to_string(entry.hotCueNumber)) :
                    entry.comment;
            }
        }
    }

    // Check for minute markers
    const float timeMs = ratio * totalMs;
    const int minutes = static_cast<int>(timeMs / 60000.0f);
    const float minuteRatio = static_cast<float>(minutes * 60000.0f) / totalMs;
    const float minuteX = componentPos.x + minuteRatio * componentSize.x;
    if (minutes > 0 && std::abs(mousePos.x - minuteX) < 3.0f) {
        return std::to_string(minutes) + ":00";
    }

    return "";
}

struct WaveformDetailDrawOptions {
    float width{0.0f};
    float height{0.0f};
    int scale{0};
    beatlink::data::Color backgroundColor{0, 0, 0, 255};
    beatlink::data::Color indicatorColor{255, 255, 255, 200};
    beatlink::data::Color emphasisColor{255, 64, 64, 230};
    bool drawBeats{true};
    bool drawCues{true};
    bool drawPlaybackPosition{true};
    OverlayPainter overlayPainter{nullptr};
};

inline void DrawWaveformDetail(const char* id,
                               const beatlink::data::WaveformDetail& detail,
                               const beatlink::data::TrackMetadata* metadata,
                               const beatlink::data::BeatGrid* beatGrid,
                               const beatlink::data::CueList* cues,
                               float playbackPositionMs,
                               const WaveformDetailDrawOptions& options = {}) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float width = (options.width > 0.0f) ? options.width : avail.x;
    const float height = (options.height > 0.0f) ? options.height : std::max(96.0f, avail.y);
    ImVec2 size(width, height);

    ImGui::InvisibleButton(id, size);
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), toImU32(options.backgroundColor));

    const int frameCount = detail.getFrameCount();
    if (frameCount <= 0) {
        return;
    }

    const int scale = (options.scale > 0) ? options.scale : std::max(1, frameCount / static_cast<int>(width));
    const float framesPerPixel = static_cast<float>(scale);
    const float maxHeight = 31.0f;
    const float waveformBottom = pos.y + height;

    for (int x = 0; x < static_cast<int>(width); ++x) {
        const int segment = static_cast<int>(x * framesPerPixel);
        if (segment >= frameCount) {
            continue;
        }

        if (detail.getStyle() == beatlink::data::WaveformStyle::THREE_BAND) {
            float low = detail.segmentHeight(segment, scale, beatlink::data::ThreeBandLayer::LOW) / maxHeight;
            float mid = detail.segmentHeight(segment, scale, beatlink::data::ThreeBandLayer::MID) / maxHeight;
            float high = detail.segmentHeight(segment, scale, beatlink::data::ThreeBandLayer::HIGH) / maxHeight;

            float lowHeight = low * height;
            float midHeight = mid * height;
            float highHeight = high * height;

            ImVec2 x0(pos.x + x, waveformBottom);
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - lowHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::LOW)));
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - midHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::MID)));
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - highHeight),
                              toImU32(beatlink::data::threeBandLayerColor(beatlink::data::ThreeBandLayer::HIGH)));
        } else {
            const float heightValue = detail.segmentHeight(segment, scale) / maxHeight;
            const float barHeight = heightValue * height;
            ImVec2 x0(pos.x + x, waveformBottom);
            drawList->AddLine(x0, ImVec2(x0.x, waveformBottom - barHeight),
                              toImU32(detail.segmentColor(segment, scale)));
        }
    }

    const float totalMs = metadata ? static_cast<float>(metadata->getDuration() * 1000) :
                         static_cast<float>(detail.getTotalTime());

    if (options.drawBeats && beatGrid && totalMs > 0.0f) {
        for (int beat = 1; beat <= beatGrid->getBeatCount(); ++beat) {
            const float beatTime = static_cast<float>(beatGrid->getTimeWithinTrack(beat));
            const float position = beatTime / totalMs;
            const float x = pos.x + position * width;
            const auto color = (beatGrid->getBeatWithinBar(beat) == 1) ? options.emphasisColor : options.indicatorColor;
            drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height), toImU32(color));
        }
    }

    if (options.drawCues && cues && totalMs > 0.0f) {
        for (const auto& entry : cues->getEntries()) {
            const float position = static_cast<float>(entry.cueTime) / totalMs;
            const float x = pos.x + position * width;
            drawList->AddRectFilled(ImVec2(x - 1.0f, pos.y), ImVec2(x + 1.0f, pos.y + 6.0f),
                                    toImU32(options.indicatorColor));
            if (entry.isLoop) {
                const float loopPos = static_cast<float>(entry.loopTime) / totalMs;
                const float lx = pos.x + loopPos * width;
                drawList->AddRectFilled(ImVec2(lx - 1.0f, pos.y), ImVec2(lx + 1.0f, pos.y + 6.0f),
                                        toImU32(options.indicatorColor));
            }
        }
    }

    if (options.drawPlaybackPosition && totalMs > 0.0f && playbackPositionMs >= 0.0f) {
        const float position = playbackPositionMs / totalMs;
        const float x = pos.x + position * width;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height),
                          toImU32(options.emphasisColor));
    }

    // Call overlay painter if provided
    if (options.overlayPainter) {
        OverlayContext ctx;
        ctx.drawList = drawList;
        ctx.position = pos;
        ctx.size = size;
        ctx.durationMs = totalMs;
        ctx.playbackPositionMs = playbackPositionMs;
        ctx.waveformWidth = width;
        ctx.waveformMargin = 0.0f;
        options.overlayPainter(ctx);
    }
}

} // namespace beatlink::ui
