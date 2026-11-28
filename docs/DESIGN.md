# Design

## Command
Every protocol command (`SET`, `GET`, `DEL`, `EXPIRE`) is a `Command` object with a single
`execute(Store&)`. The network layer does not know what a command does, it only runs it.
A switch over command names would put all behaviour in one function that grows with every
new command.

## Factory
`CommandFactory` maps a command name to a creator function and builds the `Command` from a
text line. New commands are added with `registerCommand`, without touching existing code.
It also centralizes argument validation, so the commands themselves only hold valid data.

## Strategy
`EvictionPolicy` hides how the store chooses which key to drop when it is full. `LruPolicy`
and `LfuPolicy` are interchangeable and chosen when the `Store` is constructed. The store
only reports events (`onInsert`, `onAccess`, `onRemove`) and asks for a `victim()`.
Inheritance in `Store` itself (`LruStore`, `LfuStore`) was rejected: policy and storage
would be tied together, and combinations with expiry would multiply classes.

## Observer
`Broker` is the subject, a `Subscriber` is the observer. `Connection` implements `Subscriber`,
so a published message is written straight to the subscribed client's socket. The publisher
and the subscribers know nothing about each other, only the channel name.

- The broker holds subscribers as `weak_ptr`. A connection that is already gone is skipped
  and pruned, so no dangling pointer can be called.
- Messages are delivered after the broker lock is released. A slow client then only delays
  the publisher that is writing to it, not every other publish and subscribe.
- `Connection` serializes its socket writes with a mutex, because replies come from its own
  thread and messages from publisher threads. `subscribe` holds that mutex across the
  registration and the acknowledgement, so no message can arrive before the `subscribed` line.
- Known limit: a subscriber that stops reading eventually blocks the publisher, because the
  socket write is blocking. A bounded per-subscriber queue would fix it.

## State
A connection behaves differently once it has subscribed: only `SUBSCRIBE`, `UNSUBSCRIBE`
and `QUIT` are valid. `ConnectionState` has two implementations, `NormalState` and
`SubscribedState`, and the connection delegates every line to the current one.
The first `SUBSCRIBE` switches to the subscribed state, the last `UNSUBSCRIBE` switches back.
The alternative was a `bool subscribed_` and an `if` in the line handler; that works for two
modes but every new mode adds a branch to one growing function.
States carry no data (the subscriptions live in the connection), so they are shared
const instances. A state can therefore never delete itself while it is running.

`PUBLISH` is an ordinary command registered through `registerCommand`, with the broker
injected. `SUBSCRIBE` and `UNSUBSCRIBE` are not commands because they act on the session,
not on the store.

## Decorator (persistence)
`LoggedCommand` wraps any command and writes its line to the append-only log before the
wrapped command runs. `registerPersistence` installs it through `CommandFactory::setDecorator`
for `SET`, `DEL` and `EXPIRE`. The commands themselves know nothing about persistence, and a
server started without `--aof` simply has no decorator.

- **Write-ahead, under one lock.** `AppendOnlyLog::execute` takes the log mutex, appends the
  line, then runs the command. Two clients that set the same key are therefore applied and
  logged in the same order, so replaying the log gives the state the server had. Logging
  first also means a failed write (full disk) leaves the store untouched; the client gets an
  `ERR` reply.
- **Failed append.** If writing or `fsync` fails, the file is truncated back to its size
  before the entry. Without that, half a line would stay in the middle of the file and the
  next entry would be glued to it (`SET k` + `DEL z` becoming `SET kDEL z`).
- **Crash during a write.** A crash can leave a last line without its newline. `load()`
  ignores it and cuts it off the file, otherwise the next entry would be glued to it and
  both would be lost.
- **Compaction.** `COMPACT` takes a `Store::snapshot()` (a copy of all live keys with their
  remaining TTL), writes it as `SET`/`EXPIRE` lines to `<file>.tmp`, calls `fsync` and
  renames it over the log, then syncs the directory. `rename` is atomic, so after a crash the
  file is either the old log or the new one. The temporary file stays open and becomes the
  log's file descriptor, so nothing can fail after the rename. The log lock is held during this, so no command can slip between the
  snapshot and the rename. If anything fails the old log stays in place.
- **Lock order.** Always log mutex, then store mutex. The store never calls the log.

Known limits:
- Time-dependent commands replay at the speed of the replay, not of the original run. An
  `EXPIRE` on a key that had already expired returned `0` at the time, but on replay the key is
  still alive, so the `EXPIRE` is applied and the key reappears for its TTL.
- After a failed `fsync` the durability of earlier entries is unknown (Linux reports the error
  once). The failed entry itself is removed, the older ones are not re-checked.
- TTLs are relative. A recovered `EXPIRE k 60` counts from the moment of recovery, not from
  the original command (Redis logs absolute timestamps). `COMPACT` stores the remaining time,
  rounded up to whole seconds.
- Eviction order is not persisted. `GET` is not logged, so after a restart the LRU/LFU order
  reflects the order of the logged writes, and a store with `--capacity` may keep different
  keys than before.
- `--aof-fsync` is off by default: the data survives a process crash (it is in the OS page
  cache) but not necessarily a power loss.
- Compaction is manual. It blocks all writes while it runs.

## Dependency injection
`Store` receives its `Clock` and `EvictionPolicy` through the constructor. Tests pass a
`ManualClock` and move time forward instead of sleeping.
