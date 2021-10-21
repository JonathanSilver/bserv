# bserv

*A Boost Based C++ HTTP JSON Server.*


## Quick Start

> To build the dependencies & the project, refer to [`BUILD-Windows.md`](BUILD-Windows.md) or [`BUILD-ubuntu.md`](BUILD-ubuntu.md)

- `WebApp/bserv` contains the source code for `bserv`.
- `WebApp/WebApp` is a sample project.
- [`config-Windows.json`](config-Windows.json) and [`config-ubuntu.json`](config-ubuntu.json) are two sample config file for `WebApp`'s startup parameters. **It should be configured and renamed to `config.json` before you start `WebApp`.**
- To use `WebApp`, you should setup the database as well.

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
