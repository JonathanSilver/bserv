#include <iostream>
#include <bserv/common.hpp>
#include <boost/json.hpp>
boost::json::object hello() {
	return {
		{"hello", "world"}
	};
}
boost::json::object echo(
	boost::json::object&& params,
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (!session.count("count")) session["count"] = 0;
	session["count"] = session["count"].as_int64() + 1;
	return {
		{"id", "echo"},
		{"obj", params}
	};
}
boost::json::object echo2(
	const std::string& id) {
	return {
		{"id", "echo2"},
		{"str", id}
	};
}
boost::json::object get(
	std::shared_ptr<bserv::session_type> session_ptr) {
	bserv::session_type& session = *session_ptr;
	if (session.count("count"))
		return {
			{"id", "get"},
			{"val", session["count"]}
		};
	return {
		{"id", "get"},
		{"val", 0}
	};
}
int main()
{
	bserv::server{
		bserv::server_config{},
		{
			bserv::make_path("/hello", &hello),
			bserv::make_path("/echo", &echo,
				bserv::placeholders::json_params,
				bserv::placeholders::session),
			bserv::make_path("/echo/<str>", &echo2,
				bserv::placeholders::_1),
			bserv::make_path("/get", &get,
				bserv::placeholders::session)
		}
	};
}