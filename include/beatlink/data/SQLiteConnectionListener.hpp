#pragma once

#include <memory>

#include "SlotReference.hpp"
#include "SQLiteConnection.hpp"

namespace beatlink::data {

class SQLiteConnectionListener {
public:
    virtual ~SQLiteConnectionListener() = default;
    virtual void databaseConnected(const SlotReference& slot, const SQLiteConnection& connection) = 0;
    virtual void databaseDisconnected(const SlotReference& slot, const SQLiteConnection& connection) = 0;
};

using SQLiteConnectionListenerPtr = std::shared_ptr<SQLiteConnectionListener>;

} // namespace beatlink::data
