#pragma once

namespace beatlink {

class MasterHandoffListener {
public:
    virtual ~MasterHandoffListener() = default;
    virtual void yieldMasterTo(int deviceNumber) = 0;
    virtual void yieldResponse(int deviceNumber, bool yielded) = 0;
};

} // namespace beatlink
