# bserv

*A Boost Based High Performance C++ HTTP JSON Server.*


## Dependencies

- [Boost 1.75.0](https://www.boost.org/)
- [PostgreSQL 13.2](https://www.postgresql.org/)
- [Libpqxx 7.3.1](https://github.com/jtv/libpqxx)
- [Crypto++ 8.4.0](https://cryptopp.com/)
- CMake


## Quick Start

### Database

You can import the sample database:

- Create the database in `psql`:
  ```
  create database bserv;
  ```

- Create the table in the `shell` using a sample script:
  ```
  psql bserv < db.sql
  ```


### Routing

Configure routing in [routing.hpp](routing.hpp).


### Handlers

Write the handlers in [handlers.hpp](handlers.hpp)


## Build

In the `shell`:

- Create a directory `build`, and enter it:
  ```
  mkdir build
  cd build
  ```
- Run:
  ```
  cmake ..
  ```
- Build:
  ```
  cmake --build .
  ```


## Running

In `build`, run in `shell`:
```
./bserv
```


## Performance

This test is performed by Jmeter. The unit for throughput is Transaction per second.

|URL|bserv|
|:-:|:-:|
|`/login`|139.55|
|`/find/<user>`|958.77|

For `/login`, we intentionally slow down the attacker's speed.


### Computer Hardware:
- Intel Core i9-9900K x 4
- 16GB RAM
