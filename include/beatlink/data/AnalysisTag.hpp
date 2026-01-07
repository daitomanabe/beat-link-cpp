#pragma once

#include <format>
#include <span>
#include <string>
#include <vector>

#include "DataReference.hpp"

namespace beatlink::data {

class AnalysisTag {
public:
    AnalysisTag(DataReference reference, std::string fileExtension, std::string typeTag, std::vector<uint8_t> payload)
        : dataReference_(std::move(reference))
        , fileExtension_(std::move(fileExtension))
        , typeTag_(std::move(typeTag))
        , payload_(std::move(payload))
    {
    }

    const DataReference& getDataReference() const { return dataReference_; }
    const std::string& getFileExtension() const { return fileExtension_; }
    const std::string& getTypeTag() const { return typeTag_; }
    std::span<const uint8_t> getPayload() const { return payload_; }

    std::string toString() const {
        return std::format("AnalysisTag[dataReference:{}, fileExtension:{}, typeTag:{}, bytes:{}]",
                           dataReference_.toString(), fileExtension_, typeTag_, payload_.size());
    }

private:
    DataReference dataReference_;
    std::string fileExtension_;
    std::string typeTag_;
    std::vector<uint8_t> payload_;
};

} // namespace beatlink::data
