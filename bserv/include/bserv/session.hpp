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
		// if `key` refers to an existing session, that session will be placed in
		// `session_ptr` and this function will return `true`. otherwise, `false`.
		// NOTE: a `shared_ptr` is returned instead of a reference.
		virtual bool try_get(
			const std::string& key,
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
			: rng_{ utils::internal::get_rd_value() },
			dist_{ 0, std::numeric_limits<std::size_t>::max() } {}
		bool get_or_create(
			std::string& key,
			std::shared_ptr<session_type>& session_ptr);
		bool try_get(
			const std::string& key,
			std::shared_ptr<session_type>& session_ptr);
	};

}  // bserv

#endif  // _SESSION_HPP