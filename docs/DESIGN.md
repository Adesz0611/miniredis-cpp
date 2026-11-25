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

## Dependency injection
`Store` receives its `Clock` and `EvictionPolicy` through the constructor. Tests pass a
`ManualClock` and move time forward instead of sleeping.
