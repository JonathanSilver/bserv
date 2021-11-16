#include "pch.h"
#include "bserv/database.hpp"

namespace bserv {

    std::shared_ptr<db_connection> db_connection_manager::get_or_block() {
        // `counter_lock_` must be acquired first.
        // exchanging this statement with the next will cause dead-lock,
        // because if the request is blocked by `counter_lock_`,
        // the destructor of `db_connection` will not be able to put
        // itself back due to the `queue_lock_` has already been acquired
        // by this request!
        counter_lock_.lock();
        // `queue_lock_` is acquired so that only one thread will
        // modify the `queue_`
        std::lock_guard<std::mutex> lg{ queue_lock_ };
        std::shared_ptr<raw_db_connection_type> conn = queue_.front();
        queue_.pop();
        // if there are no connections in the `queue_`,
        // `counter_lock_` remains to be locked
        // so that the following requests will be blocked
        if (queue_.size() != 0) counter_lock_.unlock();
        return std::make_shared<db_connection>(*this, conn);
    }

    db_connection::~db_connection() {
        std::lock_guard<std::mutex> lg{ mgr_.queue_lock_ };
        mgr_.queue_.emplace(conn_);
        // if this is the first available connection back to the queue,
        // `counter_lock_` is unlocked so that the blocked requests will
        // be notified
        if (mgr_.queue_.size() == 1)
            mgr_.counter_lock_.unlock();
    }

}  // bserv