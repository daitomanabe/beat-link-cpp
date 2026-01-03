#pragma once

/**
 * Handle Pattern Implementation
 * Per INTRODUCTION_JAVA_TO_CPP.md Section 5.2
 *
 * Objects are managed by integer IDs (handles) instead of raw pointers.
 * This pattern enables:
 * - Easy serialization to JSON/OSC
 * - Safe cross-boundary passing (C++ <-> Python)
 * - Predictable memory layout (pool allocation)
 * - No dangling pointer issues
 */

#include <vector>
#include <optional>
#include <cstdint>
#include <atomic>
#include <mutex>

namespace beatlink {

/**
 * A handle is an opaque integer identifier for an object.
 * Handle 0 is reserved as "invalid/null".
 */
using Handle = uint32_t;
constexpr Handle INVALID_HANDLE = 0;

/**
 * Generation counter to detect stale handles.
 * Combines handle ID with generation to catch use-after-free.
 */
struct GenerationalHandle {
    Handle id = INVALID_HANDLE;
    uint32_t generation = 0;

    [[nodiscard]] bool isValid() const noexcept {
        return id != INVALID_HANDLE;
    }

    [[nodiscard]] bool operator==(const GenerationalHandle& other) const noexcept {
        return id == other.id && generation == other.generation;
    }
};

/**
 * Pool entry wrapping an object with its generation.
 */
template<typename T>
struct PoolEntry {
    std::optional<T> value;
    uint32_t generation = 0;
    bool active = false;
};

/**
 * Object pool with handle-based access.
 * Thread-safe for concurrent access.
 *
 * Usage:
 *   HandlePool<Device> devicePool;
 *   auto handle = devicePool.allocate(Device{...});
 *   if (auto* dev = devicePool.get(handle)) {
 *       // use dev
 *   }
 *   devicePool.release(handle);
 *
 * @tparam T The type of objects to pool
 */
template<typename T>
class HandlePool {
public:
    /**
     * Allocate a new object in the pool.
     * @param value The object to store
     * @return A generational handle to the object
     */
    [[nodiscard]] GenerationalHandle allocate(T value) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Try to reuse a released slot
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!entries_[i].active) {
                entries_[i].value = std::move(value);
                entries_[i].generation++;
                entries_[i].active = true;
                return {static_cast<Handle>(i + 1), entries_[i].generation};
            }
        }

        // Allocate new slot
        entries_.push_back(PoolEntry<T>{
            .value = std::move(value),
            .generation = 1,
            .active = true
        });
        return {static_cast<Handle>(entries_.size()), 1};
    }

    /**
     * Get a pointer to the object for a handle.
     * Returns nullptr if handle is invalid or stale.
     */
    [[nodiscard]] T* get(GenerationalHandle handle) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return getMutableUnsafe(handle);
    }

    /**
     * Get a const pointer to the object for a handle.
     */
    [[nodiscard]] const T* get(GenerationalHandle handle) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return getConstUnsafe(handle);
    }

    /**
     * Release an object from the pool.
     * The handle becomes invalid after this call.
     */
    bool release(GenerationalHandle handle) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isValidUnsafe(handle)) {
            return false;
        }

        size_t idx = handle.id - 1;
        entries_[idx].value.reset();
        entries_[idx].active = false;
        // Generation increment happens on next allocation
        return true;
    }

    /**
     * Check if a handle is valid.
     */
    [[nodiscard]] bool isValid(GenerationalHandle handle) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return isValidUnsafe(handle);
    }

    /**
     * Get the number of active objects in the pool.
     */
    [[nodiscard]] size_t activeCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& entry : entries_) {
            if (entry.active) ++count;
        }
        return count;
    }

    /**
     * Iterate over all active objects.
     * The callback receives the handle and a reference to the object.
     */
    template<typename Func>
    void forEach(Func&& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (entries_[i].active && entries_[i].value.has_value()) {
                GenerationalHandle handle{static_cast<Handle>(i + 1), entries_[i].generation};
                func(handle, *entries_[i].value);
            }
        }
    }

    /**
     * Clear all objects from the pool.
     */
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    [[nodiscard]] bool isValidUnsafe(GenerationalHandle handle) const noexcept {
        if (handle.id == INVALID_HANDLE || handle.id > entries_.size()) {
            return false;
        }
        const auto& entry = entries_[handle.id - 1];
        return entry.active && entry.generation == handle.generation;
    }

    [[nodiscard]] T* getMutableUnsafe(GenerationalHandle handle) noexcept {
        if (!isValidUnsafe(handle)) {
            return nullptr;
        }
        return &(*entries_[handle.id - 1].value);
    }

    [[nodiscard]] const T* getConstUnsafe(GenerationalHandle handle) const noexcept {
        if (!isValidUnsafe(handle)) {
            return nullptr;
        }
        return &(*entries_[handle.id - 1].value);
    }

    mutable std::mutex mutex_;
    std::vector<PoolEntry<T>> entries_;
};

/**
 * Type alias for device handles.
 */
using DeviceHandle = GenerationalHandle;
using ListenerHandle = GenerationalHandle;

} // namespace beatlink
