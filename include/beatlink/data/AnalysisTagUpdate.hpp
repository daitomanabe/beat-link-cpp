#pragma once

#include <fmt/format.h>
#include <memory>
#include <string>

#include "AnalysisTag.hpp"

namespace beatlink::data {

class AnalysisTagUpdate {
public:
    AnalysisTagUpdate(int player,
                      std::string fileExtension,
                      std::string typeTag,
                      std::shared_ptr<AnalysisTag> analysisTag)
        : player(player)
        , fileExtension(std::move(fileExtension))
        , typeTag(std::move(typeTag))
        , analysisTag(std::move(analysisTag))
    {
    }

    int player;
    std::string fileExtension;
    std::string typeTag;
    std::shared_ptr<AnalysisTag> analysisTag;

    std::string toString() const {
        return fmt::format("AnalysisTagUpdate[player:{}, fileExtension:{}, typeTag:{}, analysisTag:{}]",
                           player, fileExtension, typeTag,
                           analysisTag ? analysisTag->toString() : "null");
    }
};

} // namespace beatlink::data
