/**
 * bserv - Boost-based HTTP Server
 *
 * reference:
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/server/async/http_server_async.cpp
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/server/coro/http_server_coro.cpp
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/advanced/server/advanced_server.cpp
 *
 * websocket:
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/websocket/server/async/websocket_server_async.cpp
 *
 */

#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>
#include <boost/json.hpp>

#include <memory>

#include "config.hpp"
#include "router.hpp"
#include "database.hpp"
#include "session.hpp"

namespace bserv {

	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace websocket = beast::websocket;
	namespace asio = boost::asio;
	namespace json = boost::json;
	using asio::ip::tcp;

	class server {
	private:
		// io_context for all I/O
		asio::io_context ioc_;
		router routes_;
		router ws_routes_;
		std::shared_ptr<session_manager_base> session_mgr_;
		std::shared_ptr<db_connection_manager> db_conn_mgr_;
	public:
		server(const server_config& config, router&& routes, router&& ws_routes = {});
	};

}  // bserv

#endif  // _SERVER_HPP