#pragma once

#include <memory>

#include "Database.hpp"
#include "SlotReference.hpp"

namespace beatlink::data {

class DatabaseListener {
public:
    virtual ~DatabaseListener() = default;
    virtual void databaseMounted(const SlotReference& slot, const Database& database) = 0;
    virtual void databaseUnmounted(const SlotReference& slot, const Database& database) = 0;
};

using DatabaseListenerPtr = std::shared_ptr<DatabaseListener>;

} // namespace beatlink::data
