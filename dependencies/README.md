# Dependencies


## [Boost](https://www.boost.org/)

CMD:
```
git clone --single-branch --branch master --recursive https://github.com/boostorg/boost.git
cd boost
bootstrap
b2
```


## [Crypto++](https://cryptopp.com/)

CMD:
```
git clone https://github.com/weidai11/cryptopp.git
```

1. Go to `cryptopp`.
2. Use VS2019 to open `cryptest.sln`.
3. For `Debug` `x64` configuration, open `Properties` of `cryptlib` project. In `C/C++` `Code Generation`, set `Runtime Library` to `Multithreading Debug DLL (/MDd)`.
4. For `Release` `x64` configuration, open `Properties` of `cryptlib` project. In `C/C++` `Code Generation`, set `Runtime Library` to `Multithreading DLL (/MD)`.
5. `Batch Build` `Debug` AND `Release` `x64` of `cryptlib`.


# [PostgreSQL 14.0](https://www.postgresql.org/)

1. Use this [link](https://get.enterprisedb.com/postgresql/postgresql-14.0-1-windows-x64-binaries.zip) to download the binaries.
2. Unzip the zip archive here. It should be named `pgsql` and contains `bin`, `include` and `lib`.


# [Libpqxx](https://github.com/jtv/libpqxx)

CMD:
```
git clone https://github.com/jtv/libpqxx.git
```

1. Go to `libpqxx`.
2. Use `cmake-gui`:
   - `Browse Source...` and `Browse Build...` to the root directory of `libpqxx`.
   - `Add Entry`: `PostgreSQL_INCLUDE_DIR` (`PATH`) = `../../pgsql/include`
   - `Add Entry`: `PostgreSQL_LIBRARY` (`FILEPATH`) = `../../pgsql/lib/libpq`
   - `Configure`: Use default settings (`VS2019` `x64`).
   - `Generate`
3. Use VS2019 to open `libpqxx.sln`.
4. `Batch Build` `Debug` AND `Release` `x64` of `pqxx`.


# [inja](https://github.com/pantor/inja)

CMD:
```
git clone https://github.com/pantor/inja.git
```
