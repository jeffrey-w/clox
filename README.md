# Lox

Lox is a high-level programming language supporting the functional and object-oriented paradigms, and is memory managed. It is an implmentation of the language described [here](http://craftinginterpreters.com/) written in C.

Lox boasts very few features and an almost non-existant standard library. It may be best described as an austere ECMAscript.

Some additional features not implemented by the previously referenced standard include:

* Arrays
* Native exponentiation
* String concatenation with other native types
* Pre- and post-increment/decrement operators (in progress)

# Build Instructions

Lox may be built from the command line using CMake.

Navigate to your desired output directory and enter the following commands:

```
cmake -G [desired build files] <relative path to CMakeLists.txt>
cmake --build . --target lox
```
