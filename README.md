# InterfaceConfigAPI

C++20 in-memory desired-state model for a Linux gateway. Supports Ethernet,
VLANs, bridges, IPv4/IPv6 prefixes, and administrative state. No daemon,
netlink/ioctl backend, JSON, or thread synchronization is included.

## Build and run

Requires Linux, a C++20 compiler, CMake 3.20+, and Make. No external libraries.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --test-dir build/debug --output-on-failure
./build/debug/interface_config_example
```

Replace `debug` with `release` for an optimized build. Without presets:

```sh
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual --parallel
ctest --test-dir build/manual --output-on-failure
```

## API and design

[Public header](include/interface_config/configuration.hpp),
[implementation](src/configuration.cpp), and [example](examples/main.cpp).

`Configuration` owns a map keyed by stable interface names. `Interface` holds
shared properties and a `std::variant` of Ethernet, VLAN, or Bridge properties.
References use names rather than pointers, so copies remain self-contained.
`get()` returns an editable snapshot; change it and pass it to `update()`.
`interfaces()` exposes a const view for enumeration. Its element references and
iterators must not be retained across successful mutations.

`add()` and `update()` construct a private candidate, check all local and graph
constraints, then commit with a nonthrowing swap. Validation is internal to each
mutation: callers never need a separate validation pass, and no invalid state
becomes observable. Failures, including allocation failures, leave the original
configuration unchanged. Scanning and copying a few hundred interfaces favors
clarity over indexes and rollback machinery. `remove()` checks inbound references
before erasing. Mutation errors throw `std::invalid_argument`; missing `get()`
throws `std::out_of_range`. Diagnostic messages identify the violated rule.

Updates replace the complete interface value and can change its type only when
all dependencies remain valid. VLAN parents must exist and be Ethernet; bridge
members must exist and have no addresses. The same checks apply when updating
parents or adding addresses to existing members. Dependents must be removed or
explicitly detached/reparented before removing their dependencies.

Additional model policies (beyond the assignment):

- Names must be nonempty and contain no NUL; renaming is intentionally absent.
- Bridge members are unique and can belong to at most one bridge. Nested bridges
  are allowed, with self-membership and dependency cycles rejected.
- Ethernet speed is a positive Mbps integer; MTU is 68–65535. These are model
  bounds, not hardware capability checks. Duplex, admin, and STP enum values are checked. Bridge STP uses `STP::on` or
  `STP::off` (the default).
- Linux `inet_pton` validates address text, with IPv4 prefixes 0–32 and IPv6
  prefixes 0–128. Host bits are retained. CIDR suffixes and zone identifiers are
  not accepted in address text; the prefix is supplied separately.
- Duplicate addresses and multiple VLANs with the same parent/ID are allowed;
  uniqueness and hardware-specific policy are outside this model.

A polymorphic hierarchy was considered, but a variant keeps the small, closed
set of types explicit and avoids virtual cloning and shared mutable ownership.
Per-property setters were also considered; whole-value updates allow several
related fields to change together without intermediate states. Mutable pointers
into stored interfaces are deliberately unavailable.

## Adding Bond/LAG

Add a `Bond` value type (members and bonding mode) to `Properties`. Extend the
internal dependency helper and validation with member existence, exclusivity,
address rules, and allowed member types. The generic cycle check and removal
protection then cover its dependencies. Add examples and tests for creation,
updates, type changes, and teardown. No ownership or mutation API changes are
needed. A future model needing multi-interface atomic transitions could expose
an explicit batch transaction using the same candidate-and-commit mechanism.

Tests exercise invalid insertions and updates, unchanged state after rejection,
address boundaries, dependency removal, bridge membership, cycles, detached
snapshots, reparenting, and successful teardown. Checks remain enabled in Release.
