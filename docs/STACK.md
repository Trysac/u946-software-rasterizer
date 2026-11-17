# Stack

## Used

| Tool | Why |
|---|---|
| C++20 | Standard for current systems programming. |
| CMake + Ninja | Portable build configuration. |
| clang | Single toolchain (compilation, tidy, format, cov). |
| Catch2 | Unified framework for testing and property generation. |
| vcpkg | Pinning direct dependencies via baseline. |

## Discarded

| Tool | Why not |
|---|---|
| Conan | Adds overhead (Python) for minimal benefits over vcpkg. |
| GoogleTest | Catch2 provides sufficient matchers/generators in one dependency. |
| gcovr | llvm-cov is integrated into the clang toolchain. |