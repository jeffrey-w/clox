# Build Instructions

Lox is built with CMake.

In your console, navigate to the directory in which you want your build files to be generated, or create it using `mkdir`.

Type in the following commands:

```
cmake -G "Unix Makefiles" <relative path to CMakeLists.txt>
cmake --build . --target lox
```

The executable binary file will be located at the current directory.
