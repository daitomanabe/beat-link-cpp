#pragma once

#include <fmt/format.h>
#include <string>

namespace beatlink::data {

class SearchableItem {
public:
    SearchableItem(int id, std::string label)
        : id_(id)
        , label_(std::move(label))
    {
    }

    int getId() const { return id_; }
    const std::string& getLabel() const { return label_; }

    std::string toString() const {
        return fmt::format("SearchableItem[id:{}, label:{}]", id_, label_);
    }

    bool operator==(const SearchableItem& other) const {
        return id_ == other.id_ && label_ == other.label_;
    }

    bool operator!=(const SearchableItem& other) const {
        return !(*this == other);
    }

private:
    int id_;
    std::string label_;
};

} // namespace beatlink::data
