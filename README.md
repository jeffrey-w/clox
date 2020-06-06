# Lox

cLox is a high-level programming language supporting paradigms such as object-oriented and functional programming. It is an implmentation of [this link](http://craftinginterpreters.com/) in C.

# Build Instructions

Lox may be built from the command line using CMake.

Navigate to your desired output directory, and enter the following commands:

```
cmake -G [desired build files] <relative path to CMakeLists.txt>
cmake --build . --target lox
```
