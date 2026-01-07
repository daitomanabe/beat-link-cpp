#pragma once

namespace beatlink {

class SyncListener {
public:
    virtual ~SyncListener() = default;
    virtual void setSyncMode(bool synced) = 0;
    virtual void becomeMaster() = 0;
};

} // namespace beatlink
