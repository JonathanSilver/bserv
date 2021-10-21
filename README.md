# bserv

*A Boost Based C++ HTTP JSON Server.*


## Quick Start

> To build the dependencies & the project, refer to [`BUILD-Windows.md`](BUILD-Windows.md) or [`BUILD-ubuntu.md`](BUILD-ubuntu.md).

- `WebApp/bserv` contains the source code for `bserv`.


### Hello, World!

*This [example](examples/hello-world) shows how to use `bserv` to echo a json object `{"msg": "hello, world!"}` when requesting `localhost:8080/hello`.*

`main.cpp`:
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

`CMakeLists.txt`:
```
cmake_minimum_required(VERSION 3.10)

project(hello)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(path/to/WebApp/bserv bserv)
add_executable(main main.cpp)
target_link_libraries(main PUBLIC bserv)
```


### Sample Project: `WebApp`

- `WebApp/WebApp` is a sample project.
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
