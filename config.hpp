#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include <iostream>
#include <string>
#include <cstddef>
#include <cstring>

namespace bserv {

const char* NAME = "bserv";

unsigned short PORT = 8080;
int NUM_THREADS = 4;
int NUM_DB_CONN = 10;

std::size_t PAYLOAD_LIMIT = 1 * 1024 * 1024;

std::size_t LOG_ROTATION_SIZE = 4 * 1024 * 1024;
std::string LOG_PATH = "./log/";
std::string DB_CONN_STR = "dbname=bserv";

void show_usage() {
    std::cout << "Usage: " << NAME << " [OPTION...]\n"
        << NAME << " is a C++ Boost-based HTTP server.\n\n"
        "Example:\n"
        << "  " << NAME << " -p 8081 --threads 2\n\n"
        "Option:\n"
        "  -h, --help      show help and exit\n"
        "  -p, --port      port (default: 8080)\n"
        "      --threads   number of threads (default: 4)\n"
        "      --num-conn  number of database connections (default: 10)\n"
        "      --payload   payload limit for request in mega bytes (default: 1)\n"
        "      --rotation  log rotation size in mega bytes (default: 4)\n"
        "      --log-path  log path (default: ./log/)\n"
        "  -c, --conn-str  connection string (default: dbname=bserv)"
        << std::endl;
}

// returns `true` if error occurs
bool parse_arguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_usage();
            return true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                PORT = atoi(argv[i + 1]);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                NUM_THREADS = atoi(argv[i + 1]);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--num-conn") == 0) {
            if (i + 1 < argc) {
                NUM_DB_CONN = atoi(argv[i + 1]);
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--payload") == 0) {
            if (i + 1 < argc) {
                PAYLOAD_LIMIT = atoi(argv[i + 1]) * 1024 * 1024;
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--rotation") == 0) {
            if (i + 1 < argc) {
                LOG_ROTATION_SIZE = atoi(argv[i + 1]) * 1024 * 1024;
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "--log-path") == 0) {
            if (i + 1 < argc) {
                LOG_PATH = argv[i + 1];
                if (LOG_PATH.back() != '/')
                    LOG_PATH += '/';
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--conn-str") == 0) {
            if (i + 1 < argc) {
                DB_CONN_STR = argv[i + 1];
                ++i;
            } else {
                std::cerr << "Missing value after: " << argv[i] << std::endl;
                return true;
            }
        } else {
            std::cerr << "Unrecognized option: " << argv[i] << '\n' << std::endl;
            show_usage();
            return true;
        }
    }
    return false;
}

}  // bserv

#endif  // _CONFIG_HPP