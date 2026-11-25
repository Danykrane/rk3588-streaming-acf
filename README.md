# Streaming ACF

Streaming autocorrelation in C++20: 2 kHz input, 500 frequency parameters
from 1 to 500 Hz, and a ring matrix holding the last 2000 rows.
Both direct computation and a recursive update of the running sum are available.

For frequency f the window is T = 1/f and the lag is T/4. A fractional lag
is linearly interpolated, and the edge sample of the window is weighted in proportion
to its fractional part. This is a windowed ACF, not a power spectrum or a filter bank.

## Build and test

Requires CMake 3.23+, Ninja and a C++20 compiler with `std::from_chars`
support for floating-point numbers. The range-v3 and doctest dependencies are pinned as submodules.

```sh
git submodule update --init --recursive
cmake --preset mac-release
cmake --build --preset mac-release
ctest --preset mac-release
```

On Linux use `linux-release`. The `mac-debug`, `linux-debug`, `mac-asan`
and `linux-asan` presets are also available. ASan/UBSan needs a compatible
Clang/GCC; on macOS 26.6.2 it was verified with Clang 22.1.1. The system AppleClang 17
on this OS hung while initializing ASan. Select the compiler at
configure time with `-DCMAKE_CXX_COMPILER=/path/to/clang++`.

## Usage

```sh
./build/Darwin/mac-release/acf_cli < samples.txt > acf.csv
./build/Darwin/mac-release/acf_cli --int64 --recursive < samples.txt > acf.csv
```

Input is whitespace-separated numbers. By default finite double values
in [-1; 1] are accepted. The `--int64` flag accepts signed integers
and scales them by 2⁻⁶³; not all 64 significant bits are preserved.
The output stays normalized: a CSV without a header, 500 columns, rows
in arrival order. `nan` is printed until the corresponding window has warmed up.

The library is available as the CMake target `acf::acf`. The processor owns its buffers
and is neither copyable nor movable. `row(age)` returns an immutable span
whose contents are overwritten after the next `2000 - age` samples.
The object must outlive the span; concurrent access requires synchronization.

## Additional checks

Python 3.10+ and Clang let you check the OpenCL kernel source on the CPU:

```sh
python3 tools/check_kernel.py
python3 -m unittest discover -s tests -v
```

`acf.cl` is a sketch of an FP32 GPU kernel. There is no integration with OpenCL or the device;
the code has not been run on a Mali-G610. The C++ implementation runs on the CPU in double precision.

Formatting: clang-format 15, configuration in `.clang-format`.

## License

[MIT](LICENSE).
