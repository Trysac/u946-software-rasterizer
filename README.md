# software-rasterizer

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/Trysac/u946-software-rasterizer/actions/workflows/ci.yml/badge.svg)](https://github.com/Trysac/u946-software-rasterizer/actions/workflows/ci.yml)

Rasterize triangles on the CPU with a top-left fill rule, z-buffer, perspective-correct texturing, Lambert lighting, OBJ loading and tolerance-based image comparison

## Problem

Describe in two or three sentences the concrete problem this utility solves, who hits it, and what
they do today without it. Replace this paragraph when the first real feature lands.

## Install

A C++20 library built with CMake. Until it is published to a vcpkg registry, add this repository
at a release tag as a subdirectory (or through `FetchContent`) and link its target:

```cmake
add_subdirectory(u946-software-rasterizer)
target_link_libraries(app PRIVATE trysac::software_rasterizer)
```

Requires a C++20 compiler and CMake 3.29 or newer. No dependencies outside the C++ standard
library.

## Usage

```cpp
#include <trysac/software_rasterizer/text.hpp>

// normalize trims surrounding whitespace and lower-cases the text.
auto name = trysac::software_rasterizer::normalize("  Hello World ");  // "hello world"

// Blank input is rejected explicitly instead of returning an empty string.
trysac::software_rasterizer::normalize("   ");  // throws BlankInputError ("input is blank")
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) and [decisions](docs/decisions/)
- [Testing](docs/TESTING.md) and [security](docs/SECURITY.md)
- [Project record](docs/PROJECT.md) and [stack](docs/STACK.md)
- [Changelog](CHANGELOG.md) and [contributing guide](CONTRIBUTING.md)

## License

[MIT](LICENSE) (c) 2026 Miguel Gomez
