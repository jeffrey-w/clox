# Lox

Lox is a high-level programming language supporting paradigms such as object-oriented and functional programming. It is an implmentation of the language described here [this](http://craftinginterpreters.com/), and is written in C.

Lox boasts very few features and a almost non-existant standard library. It may be best described as a stripped-down ECMAscript.

Some additional features not implemented by the previously referenced standard include:

* Arrays
* Exponentiation
* Pre- and post-increment/decrement operators (in progress)

# Build Instructions

Lox may be built from the command line using CMake.

Navigate to your desired output directory, and enter the following commands:

```
cmake -G [desired build files] <relative path to CMakeLists.txt>
cmake --build . --target lox
```
