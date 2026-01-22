#pragma once

#include <cstdint>
#include <fmt/format.h>
#include <string>

#include "SearchableItem.hpp"

namespace beatlink::data {

struct Color {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    std::string toString() const {
        return fmt::format("Color[r:{}, g:{}, b:{}, a:{}]", r, g, b, a);
    }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

class ColorItem : public SearchableItem {
public:
    ColorItem(int id, std::string label)
        : SearchableItem(id, std::move(label))
        , color_(colorForId(id))
        , colorName_(colorNameForId(id))
    {
    }

    const Color& getColor() const { return color_; }
    const std::string& getColorName() const { return colorName_; }

    std::string toString() const {
        return fmt::format("ColorItem[id:{}, label:{}, colorName:{}, color:{}]",
                           getId(), getLabel(), colorName_, color_.toString());
    }

    bool operator==(const ColorItem& other) const {
        return static_cast<const SearchableItem&>(*this) == static_cast<const SearchableItem&>(other) &&
               color_ == other.color_ && colorName_ == other.colorName_;
    }

    bool operator!=(const ColorItem& other) const {
        return !(*this == other);
    }

    static bool isNoColor(const Color& color) {
        return color.a == 0;
    }

    static Color colorForId(int colorId) {
        switch (colorId) {
            case 1:
                return Color{255, 175, 175, 255}; // Pink
            case 2:
                return Color{255, 0, 0, 255}; // Red
            case 3:
                return Color{255, 200, 0, 255}; // Orange
            case 4:
                return Color{255, 255, 0, 255}; // Yellow
            case 5:
                return Color{0, 255, 0, 255}; // Green
            case 6:
                return Color{0, 255, 255, 255}; // Aqua
            case 7:
                return Color{0, 0, 255, 255}; // Blue
            case 8:
                return Color{128, 0, 128, 255}; // Purple
            case 0:
            default:
                return Color{0, 0, 0, 0};
        }
    }

    static std::string colorNameForId(int colorId) {
        switch (colorId) {
            case 0:
                return "No Color";
            case 1:
                return "Pink";
            case 2:
                return "Red";
            case 3:
                return "Orange";
            case 4:
                return "Yellow";
            case 5:
                return "Green";
            case 6:
                return "Aqua";
            case 7:
                return "Blue";
            case 8:
                return "Purple";
            default:
                return "Unknown Color";
        }
    }

private:
    Color color_;
    std::string colorName_;
};

} // namespace beatlink::data
