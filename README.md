# BoundaryIntegralLib

Small C++ library providing custom MFEM boundary integrators following https://arxiv.org/abs/2508.02861.

## Build

Clone with submodules:

git clone --recurse-submodules [https://github.com/woutertonnon/BoundaryIntegralLib.git](https://github.com/woutertonnon/BoundaryIntegralLib.git)
cd BoundaryIntegralLib

Configure and build:

mkdir build && cd build
cmake ..
cmake --build .

## Structure

* `include/` – public headers
* `src/` – implementation
* `extern/` – MFEM submodule
* `tests/` – optional GoogleTest tests

## Requirements

* CMake
* C++ compiler
* MFEM (included as submodule)
* GoogleTest (optional)

## Notes

Build from a `build/` directory at the repository root.
