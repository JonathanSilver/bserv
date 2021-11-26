#include <iostream>
#include <optional>
#include <bserv/common.hpp>
#include <boost/json.hpp>
bserv::db_relation_to_object orm_test{
	bserv::make_db_field<int>("id"),
	bserv::make_db_field<std::string>("name"),
	bserv::make_db_field<bool>("active"),
	bserv::make_db_field<std::optional<std::string>>("email"),
	bserv::make_db_field<std::optional<int>>("code")
};
boost::json::object add(
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (!params.contains("name")) {
		return {
			{"missing", "name"}
		};
	}
	if (!params.contains("active")) params["active"] = true;
	if (!params.contains("email")) params["email"] = nullptr;
	if (!params.contains("code")) params["code"] = nullptr;
	bserv::db_transaction tx{ conn };
	bserv::db_result r = tx.exec(
		"insert into db_test (name, active, email, code) values (?, ?, ?, ?);",
		params["name"], params["active"], params["email"], params["code"]);
	lgdebug << r.query();
	tx.commit();
	return {
		{"successfully", "added"}
	};
}
boost::json::object update(
	const std::string& name,
	boost::json::object&& params,
	std::shared_ptr<bserv::db_connection> conn) {
	if (!params.contains("name")) {
		return {
			{"missing", "name"}
		};
	}
	if (!params.contains("active")) params["active"] = true;
	if (!params.contains("email")) params["email"] = nullptr;
	if (!params.contains("code")) params["code"] = nullptr;
	bserv::db_transaction tx{ conn };
	bserv::db_result r = tx.exec(
		"update db_test set name = ?, active = ?, email = ?, code = ? where name = ?;",
		params["name"], params["active"], params["email"], params["code"], name);
	lgdebug << r.query();
	tx.commit();
	return {
		{"successfully", "updated"}
	};
}
boost::json::object find(
	const std::string& name,
	std::shared_ptr<bserv::db_connection> conn) {
	bserv::db_transaction tx{ conn };
	bserv::db_result r = tx.exec("select * from db_test where name = ?;", name);
	lgdebug << r.query();
	auto item = orm_test.convert_to_optional(r);
	if (item) {
		return {
			{"item", item.value()}
		};
	}
	else {
		return {
			{"not", "found"}
		};
	}
}
int main()
{
	std::string config_content = bserv::utils::file::read_bin("../config.json");
	boost::json::object config_obj = boost::json::parse(config_content).as_object();
	bserv::server_config config;
	config.set_db_conn_str(config_obj["conn-str"].as_string().c_str());
	bserv::server{
		config,
		{
			bserv::make_path("/add", &add,
				bserv::placeholders::json_params,
				bserv::placeholders::db_connection_ptr),
			bserv::make_path("/update/<str>", &update,
				bserv::placeholders::_1,
				bserv::placeholders::json_params,
				bserv::placeholders::db_connection_ptr),
			bserv::make_path("/find/<str>", &find,
				bserv::placeholders::_1,
				bserv::placeholders::db_connection_ptr)
		}
	};
}