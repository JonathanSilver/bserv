# Build (Windows)

To build the dependencies & the project itself, The following must be installed:
- Microsoft Visual Studio 2019 (VS2019)
- CMake


## Dependencies

> For each dependency, CMD is assumed to be in the directory of `dependencies`. So, the first command is always changing into the directory which contains the dependency.


> **NOTE:** If you want to compile Libpqxx or install PostgreSQL locally, your OS username should NOT contain non-ASCII characters (e.g. Chinese characters). A workaround is to create a new administrator account whose username consists of only ASCII characters.


### [Boost](https://www.boost.org/)

CMD:
```
cd boost
bootstrap
b2
```


### [Crypto++](https://cryptopp.com/)

1. Go to `cryptopp`.
2. Use VS2019 to open `cryptest.sln`.
3. For `Debug` `x64` configuration, open `Properties` of `cryptlib` project. In `C/C++` `Code Generation`, set `Runtime Library` to `Multithreading Debug DLL (/MDd)`.
4. For `Release` `x64` configuration, open `Properties` of `cryptlib` project. In `C/C++` `Code Generation`, set `Runtime Library` to `Multithreading DLL (/MD)`.
5. `Batch Build` `Debug` AND `Release` `x64` of `cryptlib`.


### [PostgreSQL 14.0](https://www.postgresql.org/)

*The database may not be installed locally. You should be able to connect to it.*

1. Use this [link](https://get.enterprisedb.com/postgresql/postgresql-14.0-1-windows-x64-binaries.zip) to download the binaries.
2. Unzip the zip archive here. It should be named `pgsql` and contains `bin`, `include` and `lib`.


### [Libpqxx](https://github.com/jtv/libpqxx)

1. Go to `libpqxx`.
2. Use `cmake-gui`:
   - `Browse Source...` and `Browse Build...` to the root directory of `libpqxx`.
   - `Add Entry`: `PostgreSQL_INCLUDE_DIR` (`PATH`) = `../../pgsql/include`
   - `Add Entry`: `PostgreSQL_LIBRARY` (`FILEPATH`) = `../../pgsql/lib/libpq`
   - `Configure`: Use default settings (`VS2019` `x64`).
   - `Generate`
3. Use VS2019 to open `libpqxx.sln`.
4. `Batch Build` `Debug` AND `Release` `x64` of `pqxx`.


### [inja](https://github.com/pantor/inja)

*Nothing should be done...*


## Sample Project: `WebApp`

> Now we should go back to the root directory.

Use VS2019 to open `WebApp.sln`, which is a sample project. Remember to properly configure the database and `config.json` before you `Run` the project.

> `bserv` and `WebApp` should be built in `Debug` or `Release` (`x64`), NOT (`Win32`/`x86`).
