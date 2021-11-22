#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include <iostream>
#include <string>
#include <cstddef>
#include <optional>
#include <thread>

namespace bserv {

	const std::string NAME = "bserv";

	const unsigned short PORT = 8080;
	const int NUM_THREADS =
		std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 1;

	const std::size_t PAYLOAD_LIMIT = 8 * 1024 * 1024;
	const int EXPIRY_TIME = 30;  // seconds

	const std::size_t LOG_ROTATION_SIZE = 8 * 1024 * 1024;
	//const std::string LOG_PATH = "./log/" + NAME;
	const std::string LOG_PATH = "";

	const int NUM_DB_CONN = 10;
	//const std::string DB_CONN_STR = "dbname=bserv";
	const std::string DB_CONN_STR = "";

#ifdef _MSC_VER
	const std::size_t STACK_SIZE = 1024 * 1024;
#endif

#define decl_field(type, name, default_value) \
private: \
    std::optional<type> name##_; \
public: \
    void set_##name(std::optional<type>&& name) { name##_ = std::move(name); } \
    type get_##name() const { return name##_.has_value() ? name##_.value() : default_value; }

	struct server_config {
		decl_field(std::string, name, NAME)
		decl_field(unsigned short, port, PORT)
		decl_field(int, num_threads, NUM_THREADS)
		decl_field(std::size_t, log_rotation_size, LOG_ROTATION_SIZE)
		decl_field(std::string, log_path, LOG_PATH)
		decl_field(int, num_db_conn, NUM_DB_CONN)
		decl_field(std::string, db_conn_str, DB_CONN_STR)
	public:
		server_config() = default;
	};

#undef decl_field

}  // bserv

#endif  // _CONFIG_HPP