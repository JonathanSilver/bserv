#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include <boost/beast.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>

#include <iostream>
#include <string>
#include <cstddef>
#include <future>
#include <memory>
#include <chrono>
#include <exception>

#include "logging.hpp"

namespace bserv {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using asio::ip::tcp;

using request_type = http::request<http::string_body>;
using response_type = http::response<http::string_body>;

class request_failed_exception
    : public std::exception {
private:
    const std::string msg_;
public:
    request_failed_exception(const std::string& msg) : msg_{msg} {}
    const char* what() const noexcept { return msg_.c_str(); }
};

// https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/async/http_client_async.cpp
// https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/coro/http_client_coro.cpp

// sends one async request to a remote server
inline http::response<http::string_body> http_client_send(
        asio::io_context& ioc,
        asio::yield_context& yield,
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
    beast::error_code ec;
    tcp::resolver resolver{ioc};
    const auto results = resolver.async_resolve(host, port, yield[ec]);
    if (ec) {
        throw request_failed_exception{"http_client_session::resolver resolve: " + ec.message()};
    }
    beast::tcp_stream stream{ioc};
    // sets a timeout on the operation
    stream.expires_after(std::chrono::seconds(EXPIRY_TIME));
    // makes the connection on the IP address we get from a lookup
    stream.async_connect(results, yield[ec]);
    if (ec) {
        throw request_failed_exception{"http_client_session::stream connect: " + ec.message()};
    }
    // sets a timeout on the operation
    stream.expires_after(std::chrono::seconds(EXPIRY_TIME));
    // sends the HTTP request to the remote host
    http::async_write(stream, req, yield[ec]);
    if (ec) {
        throw request_failed_exception{"http_client_session::stream write: " + ec.message()};
    }
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    // receives the HTTP response
    http::async_read(stream, buffer, res, yield[ec]);
    if (ec) {
        throw request_failed_exception{"http_client_session::stream read: " + ec.message()};
    }
    // gracefully close the socket
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    // `not_connected` happens sometimes so don't bother reporting it
    if (ec && ec != beast::errc::not_connected) {
        // reports the error to the log!
        fail(ec, "http_client_session::stream::socket shutdown");
        // return;
    }
    // if we get here then the connection is closed gracefully
    return res;
}

inline request_type get_request(
    const std::string& host,
    const std::string& target,
    const http::verb& method,
    const boost::json::value& val) {
    request_type req;
    req.method(method);
    req.target(target);
    req.set(http::field::host, host);
    req.set(http::field::user_agent, NAME);
    req.set(http::field::content_type, "application/json");
    req.body() = boost::json::serialize(val);
    req.prepare_payload();
    return req;
}

class http_client {
private:
    asio::io_context& ioc_;
    asio::yield_context& yield_;
public:
    http_client(asio::io_context& ioc, asio::yield_context& yield)
    : ioc_{ioc}, yield_{yield} {}
    http::response<http::string_body> request(
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
        return http_client_send(ioc_, yield_, host, port, req);
    }
    boost::json::value request_for_value(
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
        return boost::json::parse(request(host, port, req).body());
    }

    response_type send(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const http::verb& method,
        const boost::json::value& val) {
        request_type req = get_request(host, target, method, val);
        return request(host, port, req);
    }
    boost::json::value send_for_value(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const http::verb& method,
        const boost::json::value& val) {
        request_type req = get_request(host, target, method, val);
        return request_for_value(host, port, req);
    }

    response_type get(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send(host, port, target, http::verb::get, val);
    }
    boost::json::value get_for_value(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send_for_value(host, port, target, http::verb::get, val);
    }
    response_type put(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send(host, port, target, http::verb::put, val);
    }
    boost::json::value put_for_value(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send_for_value(host, port, target, http::verb::put, val);
    }
    response_type post(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send(host, port, target, http::verb::post, val);
    }
    boost::json::value post_for_value(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send_for_value(host, port, target, http::verb::post, val);
    }
    response_type delete_(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send(host, port, target, http::verb::delete_, val);
    }
    boost::json::value delete_for_value(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::value& val) {
        return send_for_value(host, port, target, http::verb::delete_, val);
    }
};

}  // bserv

#endif  // _CLIENT_HPP