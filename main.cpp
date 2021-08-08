#include <iostream>
#include <cstdlib>

#include "bserv/common.hpp"
#include "handlers.hpp"

void show_usage(const bserv::server_config& config) {
    std::cout << "Usage: " << config.get_name() << " [OPTION...]\n"
        << config.get_name() << " is a C++ Boost-based HTTP server.\n\n"
        "Example:\n"
        << "  " << config.get_name() << " -p 8081 --threads 2\n\n"
        "Option:\n"
        "  -h, --help      show help and exit\n"
        "  -p, --port      port (default: 8080)\n"
        "      --threads   number of threads (default: # of cpu cores)\n"
        "      --rotation  log rotation size in mega bytes (default: 8)\n"
        "      --log-path  log path (default: ./log/bserv)\n"
        "      --num-conn  number of database connections (default: 10)\n"
        "  -c, --conn-str  connection string (default: dbname=bserv)"
        << std::endl;
}

// returns `true` if error occurs
bool parse_arguments(int argc, char* argv[], bserv::server_config& config) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_usage(config);
            return true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                config.set_port(atoi(argv[i + 1]));
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                config.set_num_threads(atoi(argv[i + 1]));
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--num-conn") == 0) {
            if (i + 1 < argc) {
                config.set_num_db_conn(atoi(argv[i + 1]));
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--rotation") == 0) {
            if (i + 1 < argc) {
                config.set_log_rotation_size(atoi(argv[i + 1]) * 1024 * 1024);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--log-path") == 0) {
            if (i + 1 < argc) {
                config.set_log_path(argv[i + 1]);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--conn-str") == 0) {
            if (i + 1 < argc) {
                config.set_db_conn_str(argv[i + 1]);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else {
            std::cerr << "Unrecognized option: " << argv[i] << '\n' << std::endl;
            show_usage(config);
            return true;
        }
    }
    return false;
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

    if (parse_arguments(argc, argv, config))
        return EXIT_FAILURE;

    show_config(config);

    bserv::server{config, {
        bserv::make_path("/", &hello,
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
            bserv::placeholders::json_params)
    }
    , {
        bserv::make_path("/echo", &ws_echo,
            bserv::placeholders::session,
            bserv::placeholders::websocket_server_ptr)
    }
    };
    
    return EXIT_SUCCESS;
}
