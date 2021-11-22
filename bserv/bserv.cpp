#include "pch.h"
#include "framework.h"

#include <iostream>
#include <string>
#include <cstddef>
#include <cstdlib>
#include <vector>
#include <optional>
#include <functional>
#include <thread>
#include <chrono>

#include "bserv/server.hpp"

#include "bserv/logging.hpp"
#include "bserv/utils.hpp"
#include "bserv/client.hpp"
#include "bserv/websocket.hpp"

namespace bserv {

	std::string get_address(const tcp::socket& socket) {
		tcp::endpoint end_point = socket.remote_endpoint();
		std::string addr = end_point.address().to_string()
			+ ':' + std::to_string(end_point.port());
		return addr;
	}

	http::response<http::string_body> handle_request(
		http::request<http::string_body>& req, router& routes,
		std::shared_ptr<websocket_session> ws_session,
		asio::io_context& ioc, asio::yield_context& yield) {

		const auto bad_request = [&req](beast::string_view why) {
			http::response<http::string_body> res{
				http::status::bad_request, req.version() };
			res.set(http::field::server, NAME);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = std::string{ why };
			res.prepare_payload();
			return res;
		};

		const auto not_found = [&req](beast::string_view target) {
			http::response<http::string_body> res{
				http::status::not_found, req.version() };
			res.set(http::field::server, NAME);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = "The requested url '"
				+ std::string{ target } + "' does not exist.";
			res.prepare_payload();
			return res;
		};

		const auto server_error = [&req](beast::string_view what) {
			http::response<http::string_body> res{
				http::status::internal_server_error, req.version() };
			res.set(http::field::server, NAME);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = "Internal server error: " + std::string{ what };
			res.prepare_payload();
			return res;
		};

		boost::string_view target = req.target();
		auto pos = target.find('?');
		boost::string_view url;
		if (pos == boost::string_view::npos) url = target;
		else url = target.substr(0, pos);

		http::response<http::string_body> res{
			http::status::ok, req.version() };
		res.set(http::field::server, NAME);
		res.set(http::field::content_type, "application/json");
		res.keep_alive(req.keep_alive());

		std::optional<boost::json::value> val;
		try {
			val = routes(ioc, yield, ws_session, std::string{ url }, req, res);
		}
		catch (const url_not_found_exception& /*e*/) {
			return not_found(url);
		}
		catch (const bad_request_exception& /*e*/) {
			return bad_request("Request body is not a valid JSON string.");
		}
		catch (const std::exception& e) {
			return server_error(e.what());
		}
		catch (...) {
			return server_error("Unknown exception.");
		}

		if (val.has_value()) {
			res.body() = json::serialize(val.value());
			res.prepare_payload();
		}

		return res;
	}

	class websocket_session_server;

	void handle_websocket_request(
		std::shared_ptr<websocket_session_server>,
		std::shared_ptr<websocket_session> session,
		http::request<http::string_body>& req, router& routes,
		asio::io_context& ioc, asio::yield_context yield);

	class websocket_session_server
		: public std::enable_shared_from_this<websocket_session_server> {
	private:
		friend websocket_server;
		std::string address_;
		std::shared_ptr<websocket_session> session_;
		http::request<http::string_body> req_;
		router& routes_;
		void on_accept(beast::error_code ec) {
			if (ec) {
				fail(ec, "websocket_session_server accept");
				return;
			}
			// handles request here
			asio::spawn(
				session_->ioc_,
				std::bind(
					&handle_websocket_request,
					shared_from_this(),
					session_,
					std::ref(req_),
					std::ref(routes_),
					std::ref(session_->ioc_),
					std::placeholders::_1)
#ifdef _MSC_VER
				, boost::coroutines::attributes{ STACK_SIZE }
#endif
			);
		}
	public:
		explicit websocket_session_server(
			asio::io_context& ioc,
			tcp::socket&& socket,
			http::request<http::string_body>&& req,
			router& routes)
			: address_{ get_address(socket) },
			session_{ std::make_shared<
				websocket_session>(address_, ioc, std::move(socket)) },
			req_{ std::move(req) }, routes_{ routes } {
			lgtrace << "websocket_session_server opened: " << address_;
		}
		~websocket_session_server() {
			lgtrace << "websocket_session_server closed: " << address_;
		}
		// starts the asynchronous accept operation
		void do_accept() {
			// sets suggested timeout settings for the websocket
			session_->ws_.set_option(
				websocket::stream_base::timeout::suggested(
					beast::role_type::server));
			// sets a decorator to change the Server of the handshake
			session_->ws_.set_option(
				websocket::stream_base::decorator(
					[](websocket::response_type& res) {
						res.set(
							http::field::server,
							std::string{ BOOST_BEAST_VERSION_STRING } + " websocket-server");
					}));
			// accepts the websocket handshake
			session_->ws_.async_accept(
				req_,
				beast::bind_front_handler(
					&websocket_session_server::on_accept,
					shared_from_this()));
		}
	};

	void handle_websocket_request(
		std::shared_ptr<websocket_session_server>,
		std::shared_ptr<websocket_session> session,
		http::request<http::string_body>& req, router& routes,
		asio::io_context& ioc, asio::yield_context yield) {
		handle_request(req, routes, session, ioc, yield);
	}

	std::string websocket_server::read() {
		beast::error_code ec;
		beast::flat_buffer buffer;
		// reads a message into the buffer
		session_.ws_.async_read(buffer, yield_[ec]);
		lgtrace << "websocket_server: read from " << session_.address_;
		// this indicates that the session was closed
		if (ec == websocket::error::closed) {
			throw websocket_closed{};
		}
		if (ec) {
			fail(ec, "websocket_server read");
			throw websocket_io_exception{ "websocket_server read: " + ec.message() };
		}
		// lgtrace << "websocket_server: received text? " << ws_.got_text() << " from " << address_;
		return beast::buffers_to_string(buffer.data());
	}

	void websocket_server::write(const std::string& data) {
		beast::error_code ec;
		// ws_.text(ws_.got_text());
		session_.ws_.async_write(asio::buffer(data), yield_[ec]);
		lgtrace << "websocket_server: write to " << session_.address_;
		if (ec) {
			fail(ec, "websocket_server write");
			throw websocket_io_exception{ "websocket_server write: " + ec.message() };
		}
	}


	class http_session;

	// this function produces an HTTP response for the given
	// request. The type of the response object depends on the
	// contents of the request, so the interface requires the
	// caller to pass a generic lambda for receiving the response.
	// NOTE: `send` should be called only once!
	template <class Send>
	void handle_http_request(
		std::shared_ptr<http_session>,
		http::request<http::string_body> req,
		Send& send, router& routes, asio::io_context& ioc, asio::yield_context yield) {
		send(handle_request(req, routes, nullptr, ioc, yield));
	}

	// handles an HTTP server connection
	class http_session
		: public std::enable_shared_from_this<http_session> {
	private:
		// the function object is used to send an HTTP message.
		class send_lambda {
		private:
			http_session& self_;
		public:
			send_lambda(http_session& self)
				: self_{ self } {}
			template <bool isRequest, class Body, class Fields>
			void operator()(
				http::message<isRequest, Body, Fields>&& msg) const {
				// the lifetime of the message has to extend
				// for the duration of the async operation so
				// we use a shared_ptr to manage it.
				auto sp = std::make_shared<
					http::message<isRequest, Body, Fields>>(
						std::move(msg));
				// stores a type-erased version of the shared
				// pointer in the class to keep it alive.
				self_.res_ = sp;
				// writes the response
				http::async_write(
					self_.stream_, *sp,
					beast::bind_front_handler(
						&http_session::on_write,
						self_.shared_from_this(),
						sp->need_eof()));
			}
		} lambda_;
		asio::io_context& ioc_;
		beast::tcp_stream stream_;
		beast::flat_buffer buffer_;
		boost::optional<
			http::request_parser<http::string_body>> parser_;
		std::shared_ptr<void> res_;
		router& routes_;
		router& ws_routes_;
		const std::string address_;
		void do_read() {
			// constructs a new parser for each message
			parser_.emplace();
			// applies a reasonable limit to the allowed size
			// of the body in bytes to prevent abuse.
			parser_->body_limit(PAYLOAD_LIMIT);
			// sets the timeout.
			stream_.expires_after(std::chrono::seconds(EXPIRY_TIME));
			// reads a request using the parser-oriented interface
			http::async_read(
				stream_, buffer_, *parser_,
				beast::bind_front_handler(
					&http_session::on_read,
					shared_from_this()));
		}
		void on_read(
			beast::error_code ec,
			std::size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			lgtrace << "received " << bytes_transferred << " byte(s) from: " << address_;
			// this means they closed the connection
			if (ec == http::error::end_of_stream) {
				do_close();
				return;
			}
			if (ec) {
				fail(ec, "http_session async_read");
				return;
			}

			// sees if it is a websocket upgrade
			if (websocket::is_upgrade(parser_->get())) {
				// creates a websocket session, transferring ownership
				// of both the socket and the http request
				std::make_shared<websocket_session_server>(
					ioc_,
					stream_.release_socket(),
					parser_->release(),
					ws_routes_
					)->do_accept();
				return;
			}

			// handles the request and sends the response

			asio::spawn(
				ioc_,
				std::bind(
					&handle_http_request<send_lambda>,
					shared_from_this(),
					parser_->release(),
					std::ref(lambda_),
					std::ref(routes_),
					std::ref(ioc_),
					std::placeholders::_1)
#ifdef _MSC_VER
				// currently, it is only identified on windows
				// that the default stack size is too small
				, boost::coroutines::attributes{ STACK_SIZE }
#endif
			);
			// handle_request(parser_->release(), lambda_, routes_);

			// at this point the parser can be reset
		}
		void on_write(
			bool close, beast::error_code ec,
			std::size_t bytes_transferred) {
			boost::ignore_unused(bytes_transferred);
			// we're done with the response so delete it
			res_.reset();
			if (ec) {
				fail(ec, "http_session async_write");
				return;
			}
			lgtrace << "sent " << bytes_transferred << " byte(s) to: " << address_;
			if (close) {
				// this means we should close the connection, usually because
				// the response indicated the "Connection: close" semantic.
				do_close();
				return;
			}
			// reads another request
			do_read();
		}
		void do_close() {
			// sends a TCP shutdown
			beast::error_code ec;
			stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
			// at this point the connection is closed gracefully
			lgtrace << "socket connection closed: " << address_;
		}
	public:
		http_session(
			asio::io_context& ioc,
			tcp::socket&& socket,
			router& routes,
			router& ws_routes)
			: lambda_{ *this },
			ioc_{ ioc },
			stream_{ std::move(socket) },
			routes_{ routes },
			ws_routes_{ ws_routes },
			address_{ get_address(stream_.socket()) } {
			lgtrace << "http session opened: " << address_;
		}
		~http_session() {
			lgtrace << "http session closed: " << address_;
		}
		void run() {
			asio::dispatch(
				stream_.get_executor(),
				beast::bind_front_handler(
					&http_session::do_read,
					shared_from_this()));
		}
	};

	// accepts incoming connections and launches the sessions
	class listener
		: public std::enable_shared_from_this<listener> {
	private:
		asio::io_context& ioc_;
		tcp::acceptor acceptor_;
		router& routes_;
		router& ws_routes_;
		void do_accept() {
			acceptor_.async_accept(
				asio::make_strand(ioc_),
				beast::bind_front_handler(
					&listener::on_accept,
					shared_from_this()));
		}
		void on_accept(beast::error_code ec, tcp::socket socket) {
			if (ec) {
				fail(ec, "listener::acceptor async_accept");
			}
			else {
				lgtrace << "listener accepts: " << get_address(socket);
				std::make_shared<http_session>(
					ioc_, std::move(socket), routes_, ws_routes_)->run();
			}
			do_accept();
		}
	public:
		listener(
			asio::io_context& ioc,
			tcp::endpoint endpoint,
			router& routes,
			router& ws_routes)
			: ioc_{ ioc },
			acceptor_{ asio::make_strand(ioc) },
			routes_{ routes },
			ws_routes_{ ws_routes } {
			beast::error_code ec;
			acceptor_.open(endpoint.protocol(), ec);
			if (ec) {
				fail(ec, "listener::acceptor open");
				exit(EXIT_FAILURE);
				return;
			}
			acceptor_.set_option(
				asio::socket_base::reuse_address(true), ec);
			if (ec) {
				fail(ec, "listener::acceptor set_option");
				exit(EXIT_FAILURE);
				return;
			}
			acceptor_.bind(endpoint, ec);
			if (ec) {
				fail(ec, "listener::acceptor bind");
				exit(EXIT_FAILURE);
				return;
			}
			acceptor_.listen(
				asio::socket_base::max_listen_connections, ec);
			if (ec) {
				fail(ec, "listener::acceptor listen");
				exit(EXIT_FAILURE);
				return;
			}
		}
		void run() {
			asio::dispatch(
				acceptor_.get_executor(),
				beast::bind_front_handler(
					&listener::do_accept,
					shared_from_this()));
		}
	};


	server::server(const server_config& config, router&& routes, router&& ws_routes)
		: ioc_{ config.get_num_threads() },
		routes_{ std::move(routes) },
		ws_routes_{ std::move(ws_routes) } {
		init_logging(config);

		if (config.get_db_conn_str() != "") {
			// database connection
			try {
				db_conn_mgr_ = std::make_shared<
					db_connection_manager>(config.get_db_conn_str(), config.get_num_db_conn());
			}
			catch (const std::exception& e) {
				lgfatal << "db connection initialization failed: " << e.what() << std::endl;
				exit(EXIT_FAILURE);
			}
		}
		session_mgr_ = std::make_shared<memory_session_manager>();

		std::shared_ptr<server_resources> resources_ptr = std::make_shared<server_resources>();
		resources_ptr->session_mgr = session_mgr_;
		resources_ptr->db_conn_mgr = db_conn_mgr_;

		routes_.set_resources(resources_ptr);
		ws_routes_.set_resources(resources_ptr);

		// creates and launches a listening port
		std::make_shared<listener>(
			ioc_, tcp::endpoint{ tcp::v4(), config.get_port() }, routes_, ws_routes_)->run();

		// captures SIGINT and SIGTERM to perform a clean shutdown
		asio::signal_set signals{ ioc_, SIGINT, SIGTERM };
		signals.async_wait(
			[&](const boost::system::error_code&, int) {
				// stops the `io_context`. This will cause `run()`
				// to return immediately, eventually destroying the
				// `io_context` and all of the sockets in it.
				ioc_.stop();
			});

		lginfo << config.get_name() << " started";

		// runs the I/O service on the requested number of threads
		std::vector<std::thread> v;
		v.reserve(config.get_num_threads() - 1);
		for (int i = 1; i < config.get_num_threads(); ++i)
			v.emplace_back([&] { ioc_.run(); });
		ioc_.run();

		// if we get here, it means we got a SIGINT or SIGTERM
		lginfo << "exiting " << config.get_name();

		// blocks until all the threads exit
		for (auto& t : v) t.join();
	}

}  // bserv
