# Building

Requirements:

- cmake
- SDL2
- rtmidi (Linux, Mac only)

Tested compilers:

- msvc 19.39.33523
- clang 18.1.4
- gcc 13.2.0

Full build

```bash
git clone git@github.com:jcmoyer/Nuked-SC55.git
cd Nuked-SC55
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

If you're building a binary to only run on your local machine, consider adding
`-DCMAKE_CXX_FLAGS="-march=native -mtune=native"
-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` to the first cmake command above to
enable more optimizations.

After building, you can create a self-contained install with any required files
in their correct locations under `<path>`:

```bash
cmake --install . --prefix=<path>
```

### Windows

For builds using msvc you will most likely need to pass
`-DCMAKE_PREFIX_PATH=<path>` where `<path>` points to a directory containing
SDL2, and optionally rtmidi (only when `-DUSE_RTMIDI=ON`).

cmake is expecting to find `<path>/SDL2-X.YY.Z/cmake/sdl2-config.cmake`.

For builds in an msys2 environment, installing SDL2 via pacman should be
enough.

# Development

Requirements:

- Python 3
- [Catch2 v3.7.0](https://github.com/catchorg/Catch2) installed in
  `CMAKE_PREFIX_PATH`

There is a test suite that makes sure new commits don't change existing
behavior. It is expected that all tests pass for every commit unless either:

- Upstream modified backend behavior in a way that affects sample output, or
- We modified the renderer frontend in a way that causes different output

You can run the test suite by configuring with `-DNUKED_ENABLE_TESTS=ON` and
`-DNUKED_TEST_ROMDIR=<path>` and running:

```
ctest . -C Release
```

Note that these tests take a long time to finish individually, so you may want
to pass `-j` to run them in parallel. Currently these tests require a SC-55
(v1.21) and SC-55mk2 romset with these SHA256 hashes:

```
7e1bacd1d7c62ed66e465ba05597dcd60dfc13fc23de0287fdbce6cf906c6544 *sc55_rom1.bin
effc6132d68f7e300aaef915ccdd08aba93606c22d23e580daf9ea6617913af1 *sc55_rom2.bin
5655509a531804f97ea2d7ef05b8fec20ebf46216b389a84c44169257a4d2007 *sc55_waverom1.bin
c655b159792d999b90df9e4fa782cf56411ba1eaa0bb3ac2bdaf09e1391006b1 *sc55_waverom2.bin
334b2d16be3c2362210fdbec1c866ad58badeb0f84fd9bf5d0ac599baf077cc2 *sc55_waverom3.bin

8a1eb33c7599b746c0c50283e4349a1bb1773b5c0ec0e9661219bf6c067d2042 *rom1.bin
a4c9fd821059054c7e7681d61f49ce6f42ed2fe407a7ec1ba0dfdc9722582ce0 *rom2.bin
b0b5f865a403f7308b4be8d0ed3ba2ed1c22db881b8a8326769dea222f6431d8 *rom_sm.bin
c6429e21b9b3a02fbd68ef0b2053668433bee0bccd537a71841bc70b8874243b *waverom1.bin
5b753f6cef4cfc7fcafe1430fecbb94a739b874e55356246a46abe24097ee491 *waverom2.bin
```

`NUKED_TEST_ROMDIR` should point to a directory containing these files.
