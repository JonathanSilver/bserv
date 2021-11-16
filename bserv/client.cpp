#include "pch.h"
#include "bserv/client.hpp"
#include "bserv/logging.hpp"

#include <chrono>

namespace bserv {

    // https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/async/http_client_async.cpp
    // https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/coro/http_client_coro.cpp
    
    // sends one async request to a remote server
    http::response<http::string_body> http_client_send(
        asio::io_context& ioc,
        asio::yield_context& yield,
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
        beast::error_code ec;
        tcp::resolver resolver{ ioc };
        const auto results = resolver.async_resolve(host, port, yield[ec]);
        if (ec) {
            throw request_failed_exception{ "http_client_session::resolver resolve: " + ec.message() };
        }
        beast::tcp_stream stream{ ioc };
        // sets a timeout on the operation
        stream.expires_after(std::chrono::seconds(EXPIRY_TIME));
        // makes the connection on the IP address we get from a lookup
        stream.async_connect(results, yield[ec]);
        if (ec) {
            throw request_failed_exception{ "http_client_session::stream connect: " + ec.message() };
        }
        // sets a timeout on the operation
        stream.expires_after(std::chrono::seconds(EXPIRY_TIME));
        // sends the HTTP request to the remote host
        http::async_write(stream, req, yield[ec]);
        if (ec) {
            throw request_failed_exception{ "http_client_session::stream write: " + ec.message() };
        }
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        // receives the HTTP response
        http::async_read(stream, buffer, res, yield[ec]);
        if (ec) {
            throw request_failed_exception{ "http_client_session::stream read: " + ec.message() };
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

    request_type get_request(
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

}  // bserv
