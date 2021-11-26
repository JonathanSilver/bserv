#include "pch.h"
#include "bserv/session.hpp"

namespace bserv {

    bool memory_session_manager::get_or_create(
        std::string& key,
        std::shared_ptr<session_type>& session_ptr) {
        std::lock_guard<std::mutex> lg{ lock_ };
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
        }
        else {
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

    bool memory_session_manager::try_get(
        const std::string& key,
        std::shared_ptr<session_type>& session_ptr) {
        std::lock_guard<std::mutex> lg{ lock_ };
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
        std::size_t int_key;
        if (key.empty() || str_to_int_.count(key) == 0) {
            return false;
        }
        else {
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
        return true;
    }

}  // bserv