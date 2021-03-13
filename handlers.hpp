#ifndef _HANDLERS_HPP
#define _HANDLERS_HPP

#include <boost/json/src.hpp>

#include <string>
#include <memory>
#include <vector>
#include <optional>

#include <pqxx/pqxx>

#include "common.hpp"

// register an orm mapping (to convert the db query results into
// json objects).
// the db query results contain several rows, each has a number of
// fields. the order of `make_db_field<Type[i]>(name[i])` in the
// initializer list corresponds to these fields (`Type[0]` and
// `name[0]` correspond to field[0], `Type[1]` and `name[1]`
// correspond to field[1], ...). `Type[i]` is the type you want
// to convert the field value to, and `name[i]` is the identifier
// with which you want to store the field in the json object, so
// if the returned json object is `obj`, `obj[name[i]]` will have
// the type of `Type[i]` and store the value of field[i].
bserv::db_relation_to_object orm_user{
    bserv::make_db_field<int>("id"),
    bserv::make_db_field<std::string>("username"),
    bserv::make_db_field<std::string>("password"),
    bserv::make_db_field<bool>("is_superuser"),
    bserv::make_db_field<std::string>("first_name"),
    bserv::make_db_field<std::string>("last_name"),
    bserv::make_db_field<std::string>("email"),
    bserv::make_db_field<bool>("is_active")
};

std::optional<boost::json::object> get_user(
    pqxx::work& tx,
    const std::string& username) {
    pqxx::result r = bserv::db_exec(tx,
        "select * from auth_user where username = ?", username);
    lginfo << r.query(); // this is how you log info
    return orm_user.convert_to_optional(r);
}

std::string get_or_empty(
    boost::json::object& obj,
    const std::string& key) {
    return obj.count(key) ? obj[key].as_string().c_str() : "";
}

// if you want to manually modify the response,
// the return type should be `std::nullopt_t`,
// and the return value should be `std::nullopt`.
std::nullopt_t hello(
    bserv::response_type& response,
    std::shared_ptr<bserv::session_type> session_ptr) {
    bserv::session_type& session = *session_ptr;
    boost::json::object obj;
    if (session.count("user")) {
        auto& user = session["user"].as_object();
        obj = {
            {"msg", std::string{"welcome, "}
                + user["username"].as_string().c_str() + "!"}
        };
    } else {
        obj = {{"msg", "hello, world!"}};
    }
    // the response body is a string,
    // so the `obj` should be serialized
    response.body() = boost::json::serialize(obj);
    response.prepare_payload(); // this line is important!
    return std::nullopt;
}

// if you return a json object, the serialization
// is performed automatically.
boost::json::object user_register(
    bserv::request_type& request,
    // the json object is obtained from the request body,
    // as well as the url parameters
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn) {
    if (request.method() != boost::beast::http::verb::post) {
        throw bserv::url_not_found_exception{};
    }
    if (params.count("username") == 0) {
        return {
            {"success", false},
            {"message", "`username` is required"}
        };
    }
    if (params.count("password") == 0) {
        return {
            {"success", false},
            {"message", "`password` is required"}
        };
    }
    auto username = params["username"].as_string();
    pqxx::work tx{conn->get()};
    auto opt_user = get_user(tx, username.c_str());
    if (opt_user.has_value()) {
        return {
            {"success", false},
            {"message", "`username` existed"}
        };
    }
    auto password = params["password"].as_string();
    pqxx::result r = bserv::db_exec(tx,
        "insert into ? "
        "(?, password, is_superuser, "
        "first_name, last_name, email, is_active) values "
        "(?, ?, ?, ?, ?, ?, ?)", bserv::db_name("auth_user"),
            bserv::db_name("username"),
            username.c_str(),
            bserv::utils::security::encode_password(
                password.c_str()), false,
            get_or_empty(params, "first_name"),
            get_or_empty(params, "last_name"),
            get_or_empty(params, "email"), true);
    lginfo << r.query();
    tx.commit(); // you must manually commit changes
    return {
        {"success", true},
        {"message", "user registered"}
    };
}

boost::json::object user_login(
    bserv::request_type& request,
    boost::json::object&& params,
    std::shared_ptr<bserv::db_connection> conn,
    std::shared_ptr<bserv::session_type> session_ptr) {
    if (request.method() != boost::beast::http::verb::post) {
        throw bserv::url_not_found_exception{};
    }
    if (params.count("username") == 0) {
        return {
            {"success", false},
            {"message", "`username` is required"}
        };
    }
    if (params.count("password") == 0) {
        return {
            {"success", false},
            {"message", "`password` is required"}
        };
    }
    auto username = params["username"].as_string();
    pqxx::work tx{conn->get()};
    auto opt_user = get_user(tx, username.c_str());
    if (!opt_user.has_value()) {
        return {
            {"success", false},
            {"message", "invalid username/password"}
        };
    }
    auto& user = opt_user.value();
    if (!user["is_active"].as_bool()) {
        return {
            {"success", false},
            {"message", "invalid username/password"}
        };
    }
    auto password = params["password"].as_string();
    auto encoded_password = user["password"].as_string();
    if (!bserv::utils::security::check_password(
        password.c_str(), encoded_password.c_str())) {
        return {
            {"success", false},
            {"message", "invalid username/password"}
        };
    }
    bserv::session_type& session = *session_ptr;
    session["user"] = user;
    return {
        {"success", true},
        {"message", "login successfully"}
    };
}

boost::json::object find_user(
    std::shared_ptr<bserv::db_connection> conn,
    const std::string& username) {
    pqxx::work tx{conn->get()};
    auto user = get_user(tx, username);
    if (!user.has_value()) {
        return {
            {"success", false},
            {"message", "requested user does not exist"}
        };
    }
    user.value().erase("password");
    return {
        {"success", true},
        {"user", user.value()}
    };
}

boost::json::object user_logout(
    std::shared_ptr<bserv::session_type> session_ptr) {
    bserv::session_type& session = *session_ptr;
    if (session.count("user")) {
        session.erase("user");
    }
    return {
        {"success", true},
        {"message", "logout successfully"}
    };
}

boost::json::object send_request() {
    // post for response:
    // auto res = bserv::request::post(
    //     "localhost", "8081", "/test", {{"msg", "request"}}
    // ).get();
    // return {{"response", boost::json::parse(res.body())}};
    // -------------------------------------------------------
    // - if it takes longer than 30 seconds (by default) to
    // - get the response, this will raise a read timeout
    // -------------------------------------------------------
    // post for json response (json value, rather than json
    // object, is returned):
    auto obj = bserv::request::post_for_object(
        "localhost", "8081", "/test", {{"msg", "request"}}
    ).get();
    return {{"response", obj}};
}

#endif  // _HANDLERS_HPP