#pragma once

#include <string>
#include <format>

namespace beatlink::data {

class Database {
public:
    explicit Database(std::string sourcePath = {})
        : sourcePath_(std::move(sourcePath))
    {
    }

    const std::string& getSourcePath() const { return sourcePath_; }

    std::string toString() const {
        return std::format("Database[sourcePath:{}]", sourcePath_);
    }

private:
    std::string sourcePath_;
};

} // namespace beatlink::data
