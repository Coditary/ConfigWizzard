# ConfigWizard UI

Frontends for [configwizard-core](../configwizard-core): inline CLI and Tuinator TUI.

## Layout

```text
cli/        Sequential inline CLI (Rust)
ffi/        C FFI bridge to the core session (used by the TUI demo)
tui/        Tuinator-based terminal UI (C++)
examples/   Demo schemas and run scripts
```

## Prerequisites

Clone **configwizard-core** as a sibling directory:

```text
parent/
  configwizard-core/
  configwizard-ui/    ← this repo
```

The Rust crates resolve the core via `path = "../../configwizard-core"`.

For the TUI you also need [Tuinator](https://github.com/Coditary/Tuinator) (or your local checkout).  
Default CMake search path: `../../../../Coditary/core/Tuinator` relative to `tui/`.

```bash
cmake -S tui -B tui/build -DTUINATOR_ROOT=/path/to/Tuinator
```

## Quick start

```bash
# CLI
cargo test
./examples/run-cli-wizard.sh

# TUI (builds FFI + demo on first run)
./examples/run-tui-scaffold.sh
```

## Make targets

```bash
make cli-test          # cargo test
make tui-build         # cmake build
make tui-test          # build + ctest
make check             # cli-test + tui-test
make demo-tui          # full_demo.json
```

## Demo schemas

| File | Description |
|------|-------------|
| `examples/full_demo.json` | General feature demo |
| `examples/project_scaffold_demo.json` | Project scaffold wizard (groups, sections, conditional panes) |
| `examples/licenses_demo.json` | License picker demo |
