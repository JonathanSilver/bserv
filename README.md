# bserv

*A Boost Based C++ HTTP JSON Server.*


## Dependencies

- VS2019
- CMake
- PostgreSQL
  > *The database may not be installed locally. You should be able to connect to it.*

*Refer to [readme](dependencies/README.md) for setting up the dependencies.*


## Quick Start

Use VS2019 to open `WebApp/WebApp.sln`, which is a sample project. [`config-example.json`](config-example.json) is a sample config file for `WebApp`'s startup parameters. **It should be renamed to `config.json` before you `Run` the project.**


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
