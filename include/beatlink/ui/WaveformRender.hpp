#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "imgui.h"

#include "../data/BeatGrid.hpp"
#include "../data/CueList.hpp"
#include "../data/TrackMetadata.hpp"
#include "../data/WaveformDetail.hpp"
#include "../data/WaveformPreview.hpp"

namespace beatlink::ui {

inline ImU32 toImU32(const beatlink::data::Color& color) {
    return IM_COL32(color.r, color.g, color.b, color.a);
}

struct WaveformPreviewDrawOptions {
    float width{0.0f};
    float height{0.0f};
    beatlink::data::Color backgroundColor{0, 0, 0, 255};
    beatlink::data::Color indicatorColor{255, 255, 255, 200};
    beatlink::data::Color emphasisColor{255, 64, 64, 230};
    bool drawCues{true};
    bool drawMinuteMarkers{true};
    bool drawPlaybackPosition{true};
};

inline void DrawWaveformPreview(const char* id,
                                const beatlink::data::WaveformPreview& preview,
                                const beatlink::data::TrackMetadata* metadata,
                                const beatlink::data::CueList* cues,
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

    if (options.drawPlaybackPosition && totalMs > 0.0f && playbackPositionMs >= 0.0f) {
        const float position = playbackPositionMs / totalMs;
        const float x = pos.x + position * width;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height),
                          toImU32(options.emphasisColor));
    }
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
}

} // namespace beatlink::ui
