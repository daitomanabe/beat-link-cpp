#pragma once

// C++17 compatible span implementation
// This provides a subset of C++20 std::span functionality

#include <cstddef>
#include <type_traits>
#include <array>
#include <vector>
#include <iterator>

namespace beatlink {

inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

template <typename T, std::size_t Extent = dynamic_extent>
class span {
public:
    using element_type = T;
    using value_type = typename std::remove_cv<T>::type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type extent = Extent;

    // Constructors
    constexpr span() noexcept : data_(nullptr), size_(0) {}

    constexpr span(pointer ptr, size_type count) noexcept
        : data_(ptr), size_(count) {}

    constexpr span(pointer first, pointer last) noexcept
        : data_(first), size_(static_cast<size_type>(last - first)) {}

    template <std::size_t N>
    constexpr span(element_type (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    template <std::size_t N>
    constexpr span(std::array<value_type, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    template <std::size_t N>
    constexpr span(const std::array<value_type, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // Constructor from containers (vector, etc.)
    template <typename Container,
              typename = typename std::enable_if<
                  !std::is_array<Container>::value &&
                  std::is_convertible<
                      typename std::remove_pointer<decltype(std::declval<Container&>().data())>::type(*)[],
                      element_type(*)[]>::value>::type>
    constexpr span(Container& cont) noexcept
        : data_(cont.data()), size_(cont.size()) {}

    template <typename Container,
              typename = typename std::enable_if<
                  !std::is_array<Container>::value &&
                  std::is_convertible<
                      typename std::remove_pointer<decltype(std::declval<const Container&>().data())>::type(*)[],
                      element_type(*)[]>::value>::type,
              typename = void>
    constexpr span(const Container& cont) noexcept
        : data_(cont.data()), size_(cont.size()) {}

    constexpr span(const span& other) noexcept = default;

    template <typename U, std::size_t N,
              typename = typename std::enable_if<
                  std::is_convertible<U(*)[], element_type(*)[]>::value>::type>
    constexpr span(const span<U, N>& other) noexcept
        : data_(other.data()), size_(other.size()) {}

    constexpr span& operator=(const span& other) noexcept = default;

    // Element access
    constexpr reference operator[](size_type idx) const noexcept { return data_[idx]; }
    constexpr reference front() const noexcept { return data_[0]; }
    constexpr reference back() const noexcept { return data_[size_ - 1]; }
    constexpr pointer data() const noexcept { return data_; }

    // Iterators
    constexpr iterator begin() const noexcept { return data_; }
    constexpr iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }
    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    // Observers
    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type size_bytes() const noexcept { return size_ * sizeof(element_type); }
    constexpr bool empty() const noexcept { return size_ == 0; }

    // Subviews
    constexpr span<element_type, dynamic_extent> first(size_type count) const noexcept {
        return span<element_type, dynamic_extent>(data_, count);
    }

    constexpr span<element_type, dynamic_extent> last(size_type count) const noexcept {
        return span<element_type, dynamic_extent>(data_ + (size_ - count), count);
    }

    constexpr span<element_type, dynamic_extent> subspan(
        size_type offset, size_type count = dynamic_extent) const noexcept {
        return span<element_type, dynamic_extent>(
            data_ + offset,
            count == dynamic_extent ? size_ - offset : count);
    }

private:
    pointer data_;
    size_type size_;
};

// Helper function to create spans
template <typename T>
constexpr span<T> make_span(T* ptr, std::size_t count) noexcept {
    return span<T>(ptr, count);
}

template <typename T, std::size_t N>
constexpr span<T, N> make_span(T (&arr)[N]) noexcept {
    return span<T, N>(arr);
}

template <typename Container>
constexpr auto make_span(Container& cont) noexcept
    -> span<typename Container::value_type> {
    return span<typename Container::value_type>(cont);
}

template <typename Container>
constexpr auto make_span(const Container& cont) noexcept
    -> span<const typename Container::value_type> {
    return span<const typename Container::value_type>(cont);
}

} // namespace beatlink

// Bring span into std namespace for compatibility
namespace std {
    using beatlink::span;
    using beatlink::dynamic_extent;
}
