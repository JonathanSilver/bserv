#include <bserv/common.hpp>
#include <boost/json.hpp>
#include <string>
boost::json::object greet(
	const std::string& name)
{
	return {{"hello", name}};
}
boost::json::object greet2(
	const std::string& name1,
	const std::string& name2)
{
	return {
		{"name1", name1},
		{"name2", name2}
	};
}
boost::json::object echo(
	boost::json::object&& params)
{
	return params;
}
int main()
{
	bserv::server_config config;
	bserv::server{config, {
		bserv::make_path(
			"/greet/<str>", &greet,
			bserv::placeholders::_1),
		bserv::make_path(
			"/greet/<str>/and/<str>", &greet2,
			bserv::placeholders::_1,
			bserv::placeholders::_2),
		bserv::make_path(
			"/echo", &echo,
			bserv::placeholders::json_params)
	}};
}
