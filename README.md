# BoundaryIntegralLib (cleanup)

Small C++ library providing custom MFEM boundary / jump integrators.

## Build

Clone with submodules:

``` bash
git clone --recurse-submodules https://github.com/woutertonnon/BoundaryIntegralLib.git
cd BoundaryIntegralLib
```

Configure and build:

``` bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Structure

-   `include/` -- public headers
-   `src/` -- implementation
-   `extern/` -- MFEM submodule
-   `tests/` -- optional GoogleTest tests

## Requirements

-   CMake
-   C++ compiler
-   MFEM (included as submodule)
-   GoogleTest (optional)

## Notes

Build from a `build/` directory at the repository root.