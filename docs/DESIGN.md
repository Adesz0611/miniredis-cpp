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

## Dependency injection
`Store` receives its `Clock` and `EvictionPolicy` through the constructor. Tests pass a
`ManualClock` and move time forward instead of sleeping.
