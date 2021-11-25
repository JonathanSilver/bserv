#include <iostream>
#include <cstdlib>
#include <string>

#include <boost/json.hpp>
#include "bserv/common.hpp"

#include "rendering.h"
#include "handlers.h"

void show_usage(const bserv::server_config& config) {
	std::cout << "Usage: " << config.get_name() << " [config.json]\n"
		<< config.get_name() << " is a C++ HTTP server.\n\n"
		"Example:\n"
		<< "  " << config.get_name() << " config.json\n\n"
		<< std::endl;
}

void show_config(const bserv::server_config& config) {
	std::cout << config.get_name() << " config:"
		<< "\nport: " << config.get_port()
		<< "\nthreads: " << config.get_num_threads()
		<< "\nrotation: " << config.get_log_rotation_size() / 1024 / 1024
		<< "\nlog path: " << config.get_log_path()
		<< "\ndb-conn: " << config.get_num_db_conn()
		<< "\nconn-str: " << config.get_db_conn_str() << std::endl;
}

int main(int argc, char* argv[]) {
	bserv::server_config config;

	if (argc != 2) {
		show_usage(config);
		return EXIT_FAILURE;
	}
	if (argc == 2) {
		try {
			std::string config_content = bserv::utils::file::read_bin(argv[1]);
			//std::cout << config_content << std::endl;
			boost::json::object config_obj = boost::json::parse(config_content).as_object();
			if (config_obj.contains("port"))
				config.set_port((unsigned short)config_obj["port"].as_int64());
			if (config_obj.contains("thread-num"))
				config.set_num_threads((int)config_obj["thread-num"].as_int64());
			if (config_obj.contains("conn-num"))
				config.set_num_db_conn((int)config_obj["conn-num"].as_int64());
			if (config_obj.contains("conn-str"))
				config.set_db_conn_str(config_obj["conn-str"].as_string().c_str());
			if (config_obj.contains("log-dir"))
				config.set_log_path(std::string{ config_obj["log-dir"].as_string() });
			if (!config_obj.contains("template_root")) {
				std::cerr << "`template_root` must be specified" << std::endl;
				return EXIT_FAILURE;
			}
			else init_rendering(config_obj["template_root"].as_string().c_str());
			if (!config_obj.contains("static_root")) {
				std::cerr << "`static_root` must be specified" << std::endl;
				return EXIT_FAILURE;
			}
			else init_static_root(config_obj["static_root"].as_string().c_str());
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}
	show_config(config);

	auto _ = bserv::server{ config, {
		// rest api example
		bserv::make_path("/hello", &hello,
			bserv::placeholders::response,
			bserv::placeholders::session),
		bserv::make_path("/register", &user_register,
			bserv::placeholders::request,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr),
		bserv::make_path("/login", &user_login,
			bserv::placeholders::request,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session),
		bserv::make_path("/logout", &user_logout,
			bserv::placeholders::session),
		bserv::make_path("/find/<str>", &find_user,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::_1),
		bserv::make_path("/send", &send_request,
			bserv::placeholders::session,
			bserv::placeholders::http_client_ptr,
			bserv::placeholders::json_params),
		bserv::make_path("/echo", &echo,
			bserv::placeholders::json_params),

		// serving static files
		bserv::make_path("/statics/<path>", &serve_static_files,
			bserv::placeholders::response,
			bserv::placeholders::_1),

		// serving html template files
		bserv::make_path("/", &index_page,
			bserv::placeholders::session,
			bserv::placeholders::response),
		bserv::make_path("/form_login", &form_login,
			bserv::placeholders::request,
			bserv::placeholders::response,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session),
		bserv::make_path("/form_logout", &form_logout,
			bserv::placeholders::session,
			bserv::placeholders::response),
		bserv::make_path("/users", &view_users,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session,
			bserv::placeholders::response,
			std::string{"1"}),
		bserv::make_path("/users/<int>", &view_users,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session,
			bserv::placeholders::response,
			bserv::placeholders::_1),
		bserv::make_path("/form_add_user", &form_add_user,
			bserv::placeholders::request,
			bserv::placeholders::response,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session),
		}
		, {
			// websocket example
			bserv::make_path("/echo", &ws_echo,
				bserv::placeholders::session,
				bserv::placeholders::websocket_server_ptr)
		}
	};

	return EXIT_SUCCESS;
}
