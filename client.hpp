#ifndef _CLIENT_HPP
#define _CLIENT_HPP

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/json/src.hpp>

#include <iostream>
#include <string>
#include <cstddef>
#include <future>
#include <memory>
#include <chrono>
#include <exception>

#include "config.hpp"
#include "logging.hpp"

namespace bserv {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using asio::ip::tcp;

class request_failed_exception
    : public std::exception {
private:
    std::string msg_;
public:
    request_failed_exception(const std::string& msg) : msg_{msg} {}
    const char* what() const noexcept { return msg_.c_str(); }
};

// https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/async/http_client_async.cpp

// sends one async request to a remote server
template <typename ResponseType>
class client_session
    : public std::enable_shared_from_this<
        client_session<ResponseType>> {
private:
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    // must persist between reads
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    std::promise<ResponseType> promise_;
    void failed(const beast::error_code& ec, const std::string& what) {
        promise_.set_exception(
            std::make_exception_ptr(
                request_failed_exception{what + ": " + ec.message()}));
    }
public:
    client_session(
        asio::io_context& ioc,
        const http::request<http::string_body>& req)
    : resolver_{asio::make_strand(ioc)},
    stream_{asio::make_strand(ioc)}, req_{req} {}
    std::future<ResponseType> send(
        const std::string& host,
        const std::string& port) {
        resolver_.async_resolve(
            host, port,
            beast::bind_front_handler(
                &client_session::on_resolve,
                client_session<ResponseType>::shared_from_this()));
        return promise_.get_future();
    }
    void on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results) {
        if (ec) {
            failed(ec, "client_session::resolver resolve");
            return;
        }
        // sets a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));
        // makes the connection on the IP address we get from a lookup
        stream_.async_connect(
            results,
            beast::bind_front_handler(
                &client_session::on_connect,
                client_session<ResponseType>::shared_from_this()));
    }
    void on_connect(
        beast::error_code ec,
        tcp::resolver::results_type::endpoint_type) {
        if (ec) {
            failed(ec, "client_session::stream connect");
            return;
        }
        // sets a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));
        // sends the HTTP request to the remote host
        http::async_write(
            stream_, req_,
            beast::bind_front_handler(
                &client_session::on_write,
                client_session<ResponseType>::shared_from_this()));
    }
    void on_write(
        beast::error_code ec,
        std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            failed(ec, "client_session::stream write");
            return;
        }
        // receives the HTTP response
        http::async_read(
            stream_, buffer_, res_,
            beast::bind_front_handler(
                &client_session::on_read, 
                client_session<ResponseType>::shared_from_this()));
    }
    static_assert(std::is_same<ResponseType, http::response<http::string_body>>::value
        || std::is_same<ResponseType, boost::json::value>::value,
        "unsupported `ResponseType`");
    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            failed(ec, "client_session::stream read");
            return;
        }
        if constexpr (std::is_same<ResponseType, http::response<http::string_body>>::value) {
            promise_.set_value(std::move(res_));
        } else if constexpr (std::is_same<ResponseType, boost::json::value>::value) {
            promise_.set_value(boost::json::parse(res_.body()));
        } else { // this should never happen
            promise_.set_exception(
                std::make_exception_ptr(
                    request_failed_exception{"unsupported `ResponseType`"}));
        }
        // gracefully close the socket
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
        // `not_connected` happens sometimes so don't bother reporting it
        if (ec && ec != beast::errc::not_connected) {
            // reports the error to the log!
            fail(ec, "client_session::stream::socket shutdown");
            return;
        }
        // if we get here then the connection is closed gracefully
    }
};

class client {
private:
    asio::io_context& ioc_;
public:
    client(asio::io_context& ioc)
    : ioc_{ioc} {}
    std::future<http::response<http::string_body>> request(
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
        return std::make_shared<
            client_session<http::response<http::string_body>>
        >(ioc_, req)->send(host, port);
    }
    std::future<boost::json::value> request_for_object(
        const std::string& host,
        const std::string& port,
        const http::request<http::string_body>& req) {
        return std::make_shared<
            client_session<boost::json::value>
        >(ioc_, req)->send(host, port);
    }
};

std::shared_ptr<client> client_ptr;

namespace request {

    request_type get_request(
        const std::string& host,
        const std::string& target,
        const http::verb& method,
        const boost::json::object& obj) {
        request_type req;
        req.method(method);
        req.target(target);
        req.set(http::field::host, host);
        req.set(http::field::user_agent, NAME);
        req.set(http::field::content_type, "application/json");
        req.body() = boost::json::serialize(obj);
        req.prepare_payload();
        return req;
    }

    std::future<response_type> send(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const http::verb& method,
        const boost::json::object& obj) {
        request_type req = get_request(host, target, method, obj);
        return client_ptr->request(host, port, req);
    }

    std::future<boost::json::value> send_for_object(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const http::verb& method,
        const boost::json::object& obj) {
        request_type req = get_request(host, target, method, obj);
        return client_ptr->request_for_object(host, port, req);
    }

    std::future<response_type> get(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send(host, port, target, http::verb::get, obj);
    }

    std::future<boost::json::value> get_for_object(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send_for_object(host, port, target, http::verb::get, obj);
    }

    std::future<response_type> put(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send(host, port, target, http::verb::put, obj);
    }

    std::future<boost::json::value> put_for_object(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send_for_object(host, port, target, http::verb::put, obj);
    }

    std::future<response_type> post(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send(host, port, target, http::verb::post, obj);
    }

    std::future<boost::json::value> post_for_object(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send_for_object(host, port, target, http::verb::post, obj);
    }

    std::future<response_type> delete_(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send(host, port, target, http::verb::delete_, obj);
    }

    std::future<boost::json::value> delete_for_object(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const boost::json::object& obj) {
        return send_for_object(host, port, target, http::verb::delete_, obj);
    }

}  // request

}  // bserv

#endif  // _CLIENT_HPP