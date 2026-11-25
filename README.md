# miniredis

A multithreaded in-memory key-value server written in C++17.
Final individual project for *Production Software Development* (GKNM_MSTA092).

## Features

- Text protocol over TCP: `SET`, `GET`, `DEL`, `EXPIRE`, `SUBSCRIBE`, `PUBLISH`
- Thread-pool based connection handling
- Eviction policies (LRU / LFU / TTL)
- Pub/sub channels
- Persistence (append-only log + snapshot)

## Protocol

Plain text over TCP, one command per line, one reply line per command. Command names
are case-insensitive. The server listens on 127.0.0.1 only.

| Command | Reply |
|---|---|
| `SET key value` | `OK` |
| `GET key` | the value, or `(nil)` |
| `DEL key` | `1` if the key existed, otherwise `0` |
| `EXPIRE key seconds` | `1` if the key existed, otherwise `0` |
| `PUBLISH channel message...` | number of subscribers that received it |
| `SUBSCRIBE channel...` | `subscribed <channel> <count>` per channel |
| `UNSUBSCRIBE [channel...]` | `unsubscribed <channel> <count>` per channel (all if none given) |
| `QUIT` | `OK`, then the connection is closed |

While subscribed, a connection receives `message <channel> <text>` lines and accepts only
`SUBSCRIBE`, `UNSUBSCRIBE` and `QUIT`. Errors are reported as `ERR <reason>`.
Values are single words; `PUBLISH` joins its remaining words with single spaces.

```sh
./build/debug/miniredis_server 6380 &
printf 'SET a 1\nGET a\nQUIT\n' | nc 127.0.0.1 6380
```

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
