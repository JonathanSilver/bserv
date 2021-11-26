#include <bserv/common.hpp>
#include <boost/json.hpp>
#include <string>
#include <optional>
bserv::db_relation_to_object orm_user{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("username"),
	bserv::make_db_field<std::string>("password"),
	bserv::make_db_field<bool>("is_active"),
	bserv::make_db_field<bool>("is_superuser"),
	bserv::make_db_field<std::optional<std::string>>("first_name"),
	bserv::make_db_field<std::optional<std::string>>("last_name"),
	bserv::make_db_field<std::optional<std::string>>("email")
};
std::optional<boost::json::object> get_user(
	bserv::db_transaction& tx,
	const boost::json::value& username) {
	bserv::db_result r = tx.exec(
		"select * from ex_auth_user where username = ?;", username);
	lginfo << r.query();
	return orm_user.convert_to_optional(r);
}
std::optional<boost::json::value> get_or_empty(
	boost::json::object& obj,
	const std::string& key) {
	if (!obj.contains(key)) return std::nullopt;
	return obj[key];
}
boost::json::object greet(
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (session.count("user")) {
		boost::json::object& user = session["user"].as_object();
		boost::json::object obj;
		// the first way to check non-null (!= nullptr)
		if (user["first_name"] != nullptr && user["last_name"] != nullptr) {
			obj["welcome"] = std::string{ user["first_name"].as_string() }
			+ ' ' + std::string{ user["last_name"].as_string() };
		}
		else obj["welcome"] = user["username"];
		// the second way (!is_null())
		if (!user["email"].is_null()) {
			obj["email"] = user["email"];
		}
		return obj;
	}
	else return { {"hello", "world"} };
}
boost::json::object user_register(
	bserv::request_type& request,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (request.method() != boost::beast::http::verb::post)
		throw bserv::url_not_found_exception{};
	if (!params.contains("username")) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}
	if (!params.contains("password")) {
		return {
			{"success", false},
			{"message", "`password` is required"}
		};
	}
	auto username = params["username"];
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (opt_user) {
		return {
			{"success", false},
			{"message", "`username` existed"}
		};
	}
	auto password = params["password"].as_string();
	bserv::db_result r = tx.exec(
		"insert into ex_auth_user "
		"(username, password, is_active, is_superuser, "
		"first_name, last_name, email) values "
		"(?, ?, ?, ?, ?, ?, ?);",
		username,
		bserv::utils::security::encode_password(
			password.c_str()),
		params.contains("is_active") ? params["is_active"] : true,
		params.contains("is_superuser") ? params["is_superuser"] : false,
		get_or_empty(params, "first_name"),
		get_or_empty(params, "last_name"),
		get_or_empty(params, "email"));
	lginfo << r.query();
	tx.commit(); // commit must be done explicitly
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
	if (request.method() != boost::beast::http::verb::post)
		throw bserv::url_not_found_exception{};
	if (!params.contains("username")) {
		return {
			{"success", false},
			{"message", "`username` is required"}
		};
	}
	if (!params.contains("password")) {
		return {
			{"success", false},
			{"message", "`password` is required"}
		};
	}
	auto username = params["username"];
	bserv::db_transaction tx{ conn };
	auto opt_user = get_user(tx, username);
	if (!opt_user) {
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
boost::json::object user_logout(
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (session.count("user"))
		session.erase("user");
	return {
		{"success", true},
		{"message", "logout successfully"}
	};
}
int main() {
	std::string config_content = bserv::utils::file::read_bin("../config.json");
	boost::json::object config_obj = boost::json::parse(config_content).as_object();
	bserv::server_config config;
	config.set_db_conn_str(config_obj["conn-str"].as_string().c_str());
	bserv::server{ config, {
		bserv::make_path("/greet", &greet,
			bserv::placeholders::session),
		bserv::make_path("/register", &user_register,
			bserv::placeholders::request,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr),
		bserv::make_path("/login", &user_login,
			bserv::placeholders::request,
			bserv::placeholders::json_params,
			bserv::placeholders::db_connection_ptr,
			bserv::placeholders::session),
		bserv::make_path("/logout", &user_logout,
			bserv::placeholders::session)
	} };
}