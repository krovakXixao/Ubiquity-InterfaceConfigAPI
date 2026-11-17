# Ubiquity InterfaceConfigAPI

C++20 workspace for the network interface configuration assignment. This is a
buildable scaffold; the API and configuration container are not implemented yet.
See [the assignment](docs/assignment.md) and [implementation checklist](docs/implementation-checklist.md).

## Build and run

Requires CMake 3.20+, a C++20 compiler, and Make. On Ubuntu/Debian:

```sh
sudo apt update
sudo apt install build-essential cmake
```

Open this repository directory as the editor workspace. From its root:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/interface_config_example
```

Use the `release` preset and `./build/release/interface_config_example` for an
optimized build. Presets generate `compile_commands.json` in each build directory.

Without presets:

```sh
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual --parallel
./build/manual/interface_config_example
```

## Layout

- `include/interface_config/configuration.hpp`: public API placeholder.
- `src/configuration.cpp`: configuration container implementation placeholder.
- `examples/main.cpp`: usage example entry point.
- `docs/`: original assignment and implementation checklist.

Library target: `interface_config` (alias `interface_config::interface_config`).
Executable target: `interface_config_example`. Standard library only; compiler
warnings enabled. No external dependencies or test framework are included.

## Design notes to complete

Describe the chosen model, interface identity and references, ownership, error
reporting, and how mutations preserve validity. Explain alternatives considered
and how Bond/LAG would be added. These decisions remain open for implementation.

A daemon, system backend, JSON parsing, and thread safety are outside scope.
