# miniredis

A multithreaded in-memory key-value server written in C++17.
Final individual project for *Production Software Development* (GKNM_MSTA092).

## Features

- Text protocol over TCP: `SET`, `GET`, `DEL`, `EXPIRE`, `SUBSCRIBE`, `PUBLISH`
- Thread-pool based connection handling
- Per-key expiry (`EXPIRE`) and an optional key limit with LRU or LFU eviction
- Pub/sub channels
- Persistence: append-only log, recovery at start, `COMPACT`

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
| `COMPACT` | `OK`; rewrites the append-only file (only with `--aof`) |
| `QUIT` | `OK`, then the connection is closed |

While subscribed, a connection receives `message <channel> <text>` lines and accepts only
`SUBSCRIBE`, `UNSUBSCRIBE` and `QUIT`. Errors are reported as `ERR <reason>`.
Values are single words; `PUBLISH` joins its remaining words with single spaces.

```sh
./build/debug/miniredis_server --port 6380 &
printf 'SET a 1\nGET a\nQUIT\n' | nc 127.0.0.1 6380
```

## Running

```
miniredis_server [--port N] [--threads N] [--capacity N] [--policy lru|lfu]
                 [--aof FILE [--aof-fsync]]
```

`--capacity` limits the number of keys (0 means unlimited). When it is reached, the policy
picks the key to evict. `--threads` is the number of worker threads and also the maximum
number of clients served at the same time.

## Persistence

With `--aof FILE` every `SET`, `DEL` and `EXPIRE` is appended to the file before it is
executed, and the file is replayed when the server starts. A last line that a crash cut short
is ignored. `COMPACT` shrinks the file to the commands that rebuild the current state.
`--aof-fsync` calls `fsync` after every command. See `docs/DESIGN.md` for the limits.

```sh
./build/debug/miniredis_server --aof data.aof
```

## Design patterns

| Pattern | Where | Why |
|---|---|---|
| Command | `Command`, `SetCommand`, ... | the server runs commands without knowing what they do |
| Factory | `CommandFactory`, `makeEvictionPolicy` | one place builds and validates commands from text |
| Strategy | `EvictionPolicy` (`LruPolicy`, `LfuPolicy`) | the eviction rule is chosen at start without changing `Store` |
| Observer | `Broker` and `Subscriber` | pub/sub without publishers knowing the subscribers |
| State | `ConnectionState` | a subscribed connection accepts different commands |
| Decorator | `LoggedCommand` | persistence is added around commands without changing them |
| Dependency injection | `Clock`, `EvictionPolicy`, `Broker` | tests inject a manual clock instead of sleeping |

The reasoning and the trade-offs of each are in `docs/DESIGN.md`.

## Build and test

Requirements: a C++17 compiler, CMake >= 3.21 and Ninja. GoogleTest and CLI11 are taken from
the system if present, otherwise downloaded at configure time as pinned releases.
Optional: `doxygen` for the documentation, `gcovr` for coverage.

```sh
./build.sh            # debug build   (also: release | asan | tsan | coverage)
./test.sh asan        # build + run tests under ASan/UBSan
./test.sh tsan        # the same under ThreadSanitizer
./build/debug/miniredis_server
```

Coverage report (needs `gcovr`):

```sh
./test.sh coverage
gcovr --root . --filter 'src/' --filter 'include/' --exclude 'src/main.cpp' \
      --gcov-ignore-parse-errors=negative_hits.warn_once_per_file build/coverage
```

CI (GitHub Actions) builds and tests the debug, release, ASan and TSan presets, checks
formatting with `clang-format`, runs `clang-tidy` and requires 80% line coverage.

## Layout

| Path | Contents |
|---|---|
| `include/miniredis/` | public headers (Doxygen-documented) |
| `src/` | implementation, `main.cpp` |
| `tests/` | GoogleTest unit/integration tests |
| `cmake/` | CMake helper modules (warnings, sanitizers, coverage) |
| `docs/` | design notes (`DESIGN.md`), generated Doxygen HTML |

## Design decisions

See `docs/DESIGN.md`.

## Documentation

`doxygen Doxyfile` generates HTML into `docs/html/`.
