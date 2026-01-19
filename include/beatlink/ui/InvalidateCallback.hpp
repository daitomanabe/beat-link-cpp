#pragma once

/**
 * InvalidateCallback - Callback for region invalidation notifications
 *
 * Ported from: org.deepsymmetry.beatlink.data.RepaintDelegate
 *
 * In the Java version (Swing), RepaintDelegate was used to delegate repaint
 * calls to a host component when using soft-loaded waveforms in large lists.
 *
 * In C++ with ImGui (immediate mode GUI), this concept is different:
 * - ImGui redraws everything every frame, so there's no need for "repaint" calls
 * - However, for external rendering systems or hybrid UIs, we may need to
 *   notify that a region has changed and needs updating
 *
 * This callback is provided for compatibility and for cases where the waveform
 * component is rendered to an off-screen texture or external rendering target.
 */

#include <functional>

namespace beatlink::ui {

/**
 * Region structure representing a rectangular area that needs updating.
 */
struct InvalidateRegion {
    float x{0.0f};      ///< Left edge of the region
    float y{0.0f};      ///< Top edge of the region
    float width{0.0f};  ///< Width of the region
    float height{0.0f}; ///< Height of the region

    /**
     * Check if this region is empty (zero size).
     */
    bool isEmpty() const {
        return width <= 0.0f || height <= 0.0f;
    }

    /**
     * Check if this region contains a point.
     */
    bool contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    /**
     * Expand this region to include another region.
     */
    void unionWith(const InvalidateRegion& other) {
        if (other.isEmpty()) {
            return;
        }
        if (isEmpty()) {
            *this = other;
            return;
        }
        float right = (x + width > other.x + other.width) ? x + width : other.x + other.width;
        float bottom = (y + height > other.y + other.height) ? y + height : other.y + other.height;
        x = (x < other.x) ? x : other.x;
        y = (y < other.y) ? y : other.y;
        width = right - x;
        height = bottom - y;
    }
};

/**
 * Type alias for invalidation callbacks.
 *
 * The callback is invoked when a region of the waveform component needs updating.
 * This is primarily useful when:
 * - Rendering to an off-screen texture
 * - Integrating with non-ImGui rendering systems
 * - Implementing partial updates for performance optimization
 *
 * Example usage:
 * @code
 * auto onInvalidate = [](const InvalidateRegion& region) {
 *     // Mark the texture region as dirty
 *     myTexture.invalidate(region.x, region.y, region.width, region.height);
 * };
 * @endcode
 */
using InvalidateCallback = std::function<void(const InvalidateRegion&)>;

/**
 * Null invalidate callback that does nothing.
 */
inline const InvalidateCallback NullInvalidateCallback = [](const InvalidateRegion&) {};

/**
 * Invalidation reason enumeration for more detailed callbacks.
 */
enum class InvalidateReason {
    PlaybackPositionChanged, ///< The playback position marker moved
    TrackChanged,            ///< A new track was loaded
    CueListChanged,          ///< The cue list was updated
    WaveformChanged,         ///< The waveform data was updated
    StyleChanged,            ///< Visual style options changed
    Full                     ///< Full redraw needed
};

/**
 * Extended invalidation callback with reason information.
 */
using InvalidateCallbackEx = std::function<void(const InvalidateRegion&, InvalidateReason)>;

} // namespace beatlink::ui
