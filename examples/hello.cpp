#include <bserv/common.hpp>
#include <boost/json.hpp>
boost::json::object hello()
{
	return {{"msg", "hello, world!"}};
}
int main()
{
	bserv::server_config config;
	// config.set_port(8080);
	bserv::server{config, {
		bserv::make_path("/hello", &hello)
	}};
}
