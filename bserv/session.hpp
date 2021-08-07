#ifndef _SESSION_HPP
#define _SESSION_HPP

#include <boost/json.hpp>

#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <chrono>
#include <random>
#include <limits>

#include "utils.hpp"

namespace bserv {

const std::string SESSION_NAME = "bsessionid";

// using session_type = std::map<std::string, boost::json::value>;
using session_type = boost::json::object;

struct session_manager_base
    : std::enable_shared_from_this<session_manager_base> {
    virtual ~session_manager_base() = default;
    // if `key` refers to an existing session, that session will be placed in
    // `session_ptr` and this function will return `false`.
    // otherwise, this function will create a new session, place the created
    // session in `session_ptr`, place the session id in `key`, and return `true`.
    // this means, the returned value indicates whether a new session is created,
    // the `session_ptr` will point to a session with `key` as its session id,
    // after this function is called.
    // NOTE: a `shared_ptr` is returned instead of a reference.
    virtual bool get_or_create(
        std::string& key,
        std::shared_ptr<session_type>& session_ptr) = 0;
};

class memory_session_manager : public session_manager_base {
private:
    using time_point = std::chrono::steady_clock::time_point;
    std::mt19937 rng_;
    std::uniform_int_distribution<std::size_t> dist_;
    std::map<std::string, std::size_t> str_to_int_;
    std::map<std::size_t, std::string> int_to_str_;
    std::map<std::size_t, std::shared_ptr<session_type>> sessions_;
    // `expiry` stores <key, expiry> tuple sorted by key
    std::map<std::size_t, time_point> expiry_;
    // `queue` functions as a priority queue
    // (the front element is the smallest)
    // and stores <expiry, key> tuple sorted by
    // expiry first and then key.
    std::set<std::pair<time_point, std::size_t>> queue_;
    mutable std::mutex lock_;
public:
    memory_session_manager()
        : rng_{utils::internal::get_rd_value()},
        dist_{0, std::numeric_limits<std::size_t>::max()} {}
    bool get_or_create(
        std::string& key,
        std::shared_ptr<session_type>& session_ptr) {
        std::lock_guard<std::mutex> lg{lock_};
        time_point now = std::chrono::steady_clock::now();
        // removes the expired sessions
        while (!queue_.empty() && queue_.begin()->first < now) {
            std::size_t another_key = queue_.begin()->second;
            sessions_.erase(another_key);
            expiry_.erase(another_key);
            str_to_int_.erase(int_to_str_[another_key]);
            int_to_str_.erase(another_key);
            queue_.erase(queue_.begin());
        }
        bool created = false;
        std::size_t int_key;
        if (key.empty() || str_to_int_.count(key) == 0) {
            do {
                key = utils::generate_random_string(32);
            } while (str_to_int_.count(key) != 0);
            do {
                int_key = dist_(rng_);
            } while (int_to_str_.count(int_key) != 0);
            str_to_int_[key] = int_key;
            int_to_str_[int_key] = key;
            sessions_[int_key] = std::make_shared<session_type>();
            created = true;
        } else {
            int_key = str_to_int_[key];
            queue_.erase(
                queue_.lower_bound(
                    std::make_pair(expiry_[int_key], int_key)));
        }
        // the expiry is set to be 20 minutes from now.
        // if the session is re-visited within 20 minutes,
        // the expiry will be extended.
        expiry_[int_key] = now + std::chrono::minutes(20);
        // pushes expiry-key tuple (pair) to the queue
        queue_.emplace(expiry_[int_key], int_key);
        session_ptr = sessions_[int_key];
        return created;
    }
};

}  // bserv

#endif  // _SESSION_HPP