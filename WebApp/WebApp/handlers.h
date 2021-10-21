#pragma once

#include <boost/json.hpp>

#include <string>
#include <memory>
#include <optional>

#include "bserv/common.hpp"

std::nullopt_t hello(
    bserv::response_type& response,
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object user_register(
    bserv::request_type& request,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn);

boost::json::object user_login(
    bserv::request_type& request,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object find_user(
    std::shared_ptr<bserv::db_connection> conn,
    const std::string& username);

boost::json::object user_logout(
    std::shared_ptr<bserv::session_type> session_ptr);

boost::json::object send_request(
    std::shared_ptr<bserv::session_type> session,
    std::shared_ptr<bserv::http_client> client_ptr,
    boost::json::object&& params);

boost::json::object echo(
    boost::json::object&& params);

// websocket
std::nullopt_t ws_echo(
    std::shared_ptr<bserv::session_type> session,
    std::shared_ptr<bserv::websocket_server> ws_server);

std::nullopt_t serve_static_files(
    bserv::response_type& response,
    const std::string& path);

std::nullopt_t index_page(
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t form_login(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);

std::nullopt_t form_logout(
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response);

std::nullopt_t view_users(
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr,
    bserv::response_type& response,
    const std::string& page_num);

std::nullopt_t form_add_user(
    bserv::request_type& request,
    bserv::response_type& response,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr);
