/**
 * bserv - Boost-based HTTP Server
 * 
 * reference:
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/server/async/http_server_async.cpp
 * https://www.boost.org/doc/libs/1_75_0/libs/beast/example/advanced/server/advanced_server.cpp
 * 
 */

#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/json/src.hpp>

#include <iostream>
#include <string>
#include <cstddef>
#include <cstdlib>
#include <vector>
#include <optional>
#include <memory>
#include <thread>
#include <chrono>

#include "config.hpp"
#include "logging.hpp"
#include "utils.hpp"
#include "router.hpp"
#include "database.hpp"
#include "session.hpp"
#include "client.hpp"

namespace bserv {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using asio::ip::tcp;

// this function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
// NOTE: `send` should be called only once!
template <class Body, class Allocator, class Send>
void handle_request(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send, router& routes) {
    
    const auto bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{
            http::status::bad_request, req.version()};
        res.set(http::field::server, NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string{why};
        res.prepare_payload();
        return res;
    };

    const auto not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{
            http::status::not_found, req.version()};
        res.set(http::field::server, NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The requested url '" 
            + std::string{target} + "' does not exist.";
        res.prepare_payload();
        return res;
    };

    const auto server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{
            http::status::internal_server_error, req.version()};
        res.set(http::field::server, NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "Internal server error: " + std::string{what};
        res.prepare_payload();
        return res;
    };

    boost::string_view target = req.target();
    auto pos = target.find('?');
    boost::string_view url;
    if (pos == boost::string_view::npos) url = target;
    else url = target.substr(0, pos);

    http::response<http::string_body> res{
        http::status::ok, req.version()};
    res.set(http::field::server, NAME);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    
    std::optional<boost::json::value> val;
    try {
        val = routes(std::string{url}, req, res);
    } catch (const url_not_found_exception& e) {
        send(not_found(url));
        return;
    } catch (const bad_request_exception& e) {
        send(bad_request("Request body is not a valid JSON string."));
        return;
    } catch (const std::exception& e) {
        send(server_error(e.what()));
        return;
    } catch (...) {
        send(server_error("Unknown exception."));
        return;
    }

    if (val.has_value()) {
        res.body() = json::serialize(val.value());
        res.prepare_payload();
    }

    send(std::move(res));
}

std::string get_address(const tcp::socket& socket) {
    tcp::endpoint end_point = socket.remote_endpoint();
    std::string addr = end_point.address().to_string()
        + ':' + std::to_string(end_point.port());
    return addr;
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
            : self_{self} {}
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
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    boost::optional<
        http::request_parser<http::string_body>> parser_;
    std::shared_ptr<void> res_;
    router& routes_;
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
        // handles the request and sends the response
        handle_request(parser_->release(), lambda_, routes_);
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
    http_session(tcp::socket&& socket, router& routes)
        : lambda_{*this}, stream_{std::move(socket)}, routes_{routes},
        address_{get_address(stream_.socket())} {
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
        } else {
            lgtrace << "listener accepts: " << get_address(socket);
            std::make_shared<http_session>(
                std::move(socket), routes_)->run();
        }
        do_accept();
    }
public:
    listener(
        asio::io_context& ioc,
        tcp::endpoint endpoint,
        router& routes)
        : ioc_{ioc},
        acceptor_{asio::make_strand(ioc)},
        routes_{routes} {
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

class server {
private:
    // io_context for all I/O
    asio::io_context ioc_;
    router routes_;
    std::shared_ptr<session_manager_base> session_mgr_;
    std::shared_ptr<db_connection_manager> db_conn_mgr_;
    std::shared_ptr<http_client> http_client_ptr_;
public:
    server(const server_config& config, router&& routes)
        : ioc_{config.get_num_threads()},
        routes_{std::move(routes)} {
        init_logging(config);

        // database connection
        try {
            db_conn_mgr_ = std::make_shared<
                db_connection_manager>(config.get_db_conn_str(), config.get_num_db_conn());
        } catch (const std::exception& e) {
            lgfatal << "db connection initialization failed: " << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
        session_mgr_ = std::make_shared<memory_session_manager>();
        http_client_ptr_ = std::make_shared<http_client>(ioc_);

        std::shared_ptr<server_resources> resources_ptr = std::make_shared<server_resources>();
        resources_ptr->session_mgr = session_mgr_;
        resources_ptr->db_conn_mgr = db_conn_mgr_;
        resources_ptr->http_client_ptr = http_client_ptr_;

        routes_.set_resources(resources_ptr);

        // creates and launches a listening port
        std::make_shared<listener>(
            ioc_, tcp::endpoint{tcp::v4(), config.get_port()}, routes_)->run();

        // captures SIGINT and SIGTERM to perform a clean shutdown
        asio::signal_set signals{ioc_, SIGINT, SIGTERM};
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
            v.emplace_back([&]{ ioc_.run(); });
        ioc_.run();

        // if we get here, it means we got a SIGINT or SIGTERM
        lginfo << "exiting " << config.get_name();

        // blocks until all the threads exit
        for (auto & t : v) t.join();
    }
};

}  // bserv

#endif  // _SERVER_HPP