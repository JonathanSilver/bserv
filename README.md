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

Please refer to [this](build/README.md).


## Running

Run in `shell`:
```
./build/bserv
```


## Performance

This test is performed by Jmeter. The unit for throughput is Transaction per second.

|URL|bserv|
|:-:|:-:|
|`/login`|139.55|
|`/find/<user>`|958.77|

For `/login`, we must slow down the attacker's speed. In Java, plain password is stored, which results in a higher performance.


### Computer Hardware:
- Intel Core i9-9900K x 4
- 16GB RAM
