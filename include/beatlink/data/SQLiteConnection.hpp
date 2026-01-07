#pragma once

#include <string>
#include <format>

namespace beatlink::data {

class SQLiteConnection {
public:
    explicit SQLiteConnection(std::string sourcePath = {})
        : sourcePath_(std::move(sourcePath))
    {
    }

    const std::string& getSourcePath() const { return sourcePath_; }

    std::string toString() const {
        return std::format("SQLiteConnection[sourcePath:{}]", sourcePath_);
    }

private:
    std::string sourcePath_;
};

} // namespace beatlink::data
