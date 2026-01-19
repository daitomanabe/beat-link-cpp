#pragma once

/**
 * OverlayPainter - Interface for custom overlay rendering on waveform components
 *
 * Ported from: org.deepsymmetry.beatlink.data.OverlayPainter
 *
 * In the Java version, this was an interface allowing applications like Beat Link Trigger
 * to overlay selections or cues on the GUI components. In C++ with ImGui, this is
 * implemented as a function object (callback) that receives the ImDrawList and
 * component bounds for custom drawing.
 */

#include <functional>

#include "imgui.h"

namespace beatlink::ui {

/**
 * Context passed to overlay painters containing all necessary information
 * for rendering custom overlays on waveform components.
 */
struct OverlayContext {
    /// The ImGui draw list to use for rendering
    ImDrawList* drawList{nullptr};

    /// The screen position of the component (top-left corner)
    ImVec2 position{0.0f, 0.0f};

    /// The size of the component
    ImVec2 size{0.0f, 0.0f};

    /// Track duration in milliseconds (0 if unknown)
    float durationMs{0.0f};

    /// Current playback position in milliseconds (-1 if unknown)
    float playbackPositionMs{-1.0f};

    /// Waveform width in pixels (excluding margins)
    float waveformWidth{0.0f};

    /// Left margin of the waveform area
    float waveformMargin{0.0f};

    /**
     * Convert a time in milliseconds to an X coordinate within the component.
     *
     * @param milliseconds The time to convert
     * @return The X coordinate in screen space, or 0 if duration is unknown
     */
    float millisecondsToX(float milliseconds) const {
        if (durationMs <= 0.0f || waveformWidth <= 0.0f) {
            return position.x + waveformMargin;
        }
        float ratio = milliseconds / durationMs;
        ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
        return position.x + waveformMargin + ratio * waveformWidth;
    }

    /**
     * Convert an X coordinate to a time in milliseconds.
     *
     * @param x The X coordinate in screen space
     * @return The corresponding time in milliseconds, or 0 if duration is unknown
     */
    float xToMilliseconds(float x) const {
        if (durationMs <= 0.0f || waveformWidth <= 0.0f) {
            return 0.0f;
        }
        float relativeX = x - position.x - waveformMargin;
        return (relativeX / waveformWidth) * durationMs;
    }

    /**
     * Check if a point is within the waveform area.
     *
     * @param point The point to check
     * @return true if the point is within the waveform area
     */
    bool isPointInWaveform(const ImVec2& point) const {
        return point.x >= position.x + waveformMargin &&
               point.x <= position.x + waveformMargin + waveformWidth &&
               point.y >= position.y &&
               point.y <= position.y + size.y;
    }
};

/**
 * Type alias for overlay painter callbacks.
 *
 * The callback receives an OverlayContext containing all information needed
 * for custom rendering. The painter should use the provided ImDrawList to
 * draw any custom overlays.
 *
 * Example usage:
 * @code
 * auto myOverlay = [](const OverlayContext& ctx) {
 *     // Draw a vertical line at 30 seconds
 *     float x = ctx.millisecondsToX(30000.0f);
 *     ctx.drawList->AddLine(
 *         ImVec2(x, ctx.position.y),
 *         ImVec2(x, ctx.position.y + ctx.size.y),
 *         IM_COL32(255, 255, 0, 200)
 *     );
 * };
 * @endcode
 */
using OverlayPainter = std::function<void(const OverlayContext&)>;

/**
 * Null overlay painter that does nothing.
 * Can be used to clear an overlay painter or as a default value.
 */
inline const OverlayPainter NullOverlayPainter = [](const OverlayContext&) {};

} // namespace beatlink::ui
