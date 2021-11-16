# Build (Ubuntu)

To build the dependencies & the project itself, The following must be installed:
- g++
- make
- CMake


## Dependencies

> For each dependency, shell is assumed to be in the directory of `dependencies`. So, the first command is always changing into the directory which contains the dependency.


### [Boost](https://www.boost.org/)
shell:
```
cd boost
./bootstrap
./b2
sudo ./b2 install
```


### [Crypto++](https://cryptopp.com/)
shell:

```
cd cryptopp
make
```


### [PostgreSQL](https://www.postgresql.org/)
**The database should be installed locally.**


### [Libpqxx](https://github.com/jtv/libpqxx)
shell:
```
cd libpqxx
./configure
make
```


### [inja](https://github.com/pantor/inja)

*Nothing should be done...*


## Sample Project: `WebApp`

> Now we should go back to the root directory.

shell:
```
mkdir build && cd build
cmake ..
cmake --build .
```

Assuming the shell is in `build`, you can run `WebApp` using:
```
cd WebApp
./WebApp ../../config.json
```
given that the database and `config.json` are properly configured.

> If some of the dynamically linked libraries are missing, try:
> ```
> sudo ldconfig
> ```
