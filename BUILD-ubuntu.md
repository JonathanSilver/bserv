# Build

*Refer to [readme](dependencies/README-ubuntu.md) for setting up the dependencies.*

shell:
```
mkdir build && cd build
cmake ..
cmake --build .
```

Assuming the shell is in `build`, you can run `WebApp` using:
```
cd WebApp/WebApp
./WebApp ../../../config.json
```
given that the database and `config.json` are properly configured.

> If some of the dynamically linked libraries are missing, try:
> ```
> sudo ldconfig
> ```
