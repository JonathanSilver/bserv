# bserv

*A Boost Based C++ HTTP JSON Server.*

> NOTE:
> - To build the dependencies & the project, refer to [`BUILD-Windows.md`](BUILD-Windows.md) or [`BUILD-ubuntu.md`](BUILD-ubuntu.md).
> - `bserv` contains the source code for `bserv`.


## Quick Start

> In Windows, you can simply open [`examples/Examples.sln`](examples/Examples.sln). In Ubuntu, [`examples/CMakeLists.txt`](examples/CMakeLists.txt) can be used to compile all the examples.

### Hello, World!

[`examples/hello.cpp`](examples/hello.cpp):
```C++
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
```

By default, `bserv` listens to `8080`. When you make a request (of any type) to `localhost:8080/hello`, it will respond: `{"msg": "hello, world!"}`.


### Routing

[`examples/routing.cpp`](examples/routing.cpp):
```C++
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
```

The following table shows some requests & responses:

|Method|URL|Request Body|Response Body|
|:-:|:-:|:-:|:-:|
|Any|`/greet/world`|(Empty)|`{"hello": "world"}`|
|Any|`/greet/world1/and/world2`|(Empty)|`{"name1": "world1", "name2": "world2"}`|
|GET|`/echo?hello=world`|(Empty)|`{"hello": "world"}`|
|POST|`/echo`|`{"hello": "world"}`|`{"hello": "world"}`|

*All of the URLs should be prefixed with `localhost:8080` when you make the requests.*


### Sample Project: `WebApp`

- `WebApp` is a sample project.
- [`config-Windows.json`](config-Windows.json) and [`config-ubuntu.json`](config-ubuntu.json) are two sample config file for `WebApp`'s startup parameters. **It should be configured and renamed to `config.json` before you start `WebApp`.**
- To use `WebApp`, you should setup the database as well.

#### Database

You can import the sample database:

- Create the database in `psql`:
  ```
  create database bserv;
  ```

- Create the table in the `shell` using a sample script:
  ```
  psql bserv < db.sql
  ```
