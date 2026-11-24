# miniredis

A multithreaded in-memory key-value server written in C++17.
Final individual project for *Production Software Development* (GKNM_MSTA092).

## Features

- Text protocol over TCP: `SET`, `GET`, `DEL`, `EXPIRE`, `SUBSCRIBE`, `PUBLISH`
- Thread-pool based connection handling
- Eviction policies (LRU / LFU / TTL)
- Pub/sub channels
- Persistence (append-only log + snapshot)

## Build and test

Requirements: a C++17 compiler, CMake >= 3.21, Ninja, GoogleTest
(a system package is used if found, otherwise it is downloaded).

```sh
./build.sh            # debug build   (also: release | asan | tsan | coverage)
./test.sh asan        # build + run tests under ASan/UBSan
./build/debug/miniredis_server
```

## Layout

| Path | Contents |
|---|---|
| `include/miniredis/` | public headers (Doxygen-documented) |
| `src/` | implementation, `main.cpp` |
| `tests/` | GoogleTest unit/integration tests |
| `cmake/` | CMake helper modules (warnings, sanitizers, coverage) |
| `docs/` | design notes, generated Doxygen HTML |

## Design decisions

See `docs/DESIGN.md`.

## Documentation

`doxygen Doxyfile` generates HTML into `docs/html/`.
