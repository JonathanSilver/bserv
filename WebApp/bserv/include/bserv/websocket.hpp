#ifndef _WEBSOCKET_HPP
#define _WEBSOCKET_HPP

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <cstddef>
#include <cstdlib>

namespace bserv {

	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace websocket = beast::websocket;
	namespace asio = boost::asio;
	namespace json = boost::json;
	using asio::ip::tcp;

	class websocket_closed
		: public std::exception {
	public:
		websocket_closed() {}
		const char* what() const noexcept { return "websocket session has been closed"; }
	};

	class websocket_io_exception
		: public std::exception {
	private:
		const std::string msg_;
	public:
		websocket_io_exception(const std::string& msg) : msg_{ msg } {}
		const char* what() const noexcept { return msg_.c_str(); }
	};

	struct websocket_session {
		const std::string address_;
		asio::io_context& ioc_;
		websocket::stream<beast::tcp_stream> ws_;
		websocket_session(
			const std::string& address,
			asio::io_context& ioc,
			tcp::socket&& socket)
			: address_{ address },
			ioc_{ ioc }, ws_{ std::move(socket) } {}
	};

	class websocket_server {
	private:
		websocket_session& session_;
		asio::yield_context& yield_;
	public:
		websocket_server(websocket_session& session, asio::yield_context& yield)
			: session_{ session }, yield_{ yield } {}
		std::string read();
		boost::json::value read_json() { return boost::json::parse(read()); }
		void write(const std::string& data);
		void write_json(const boost::json::value& val) { write(boost::json::serialize(val)); }
	};

}  // bserv

#endif  // _WEBSOCKET_HPP