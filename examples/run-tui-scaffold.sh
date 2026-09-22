#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEMO_BIN="$ROOT/tui/build/examples/configwizard-tui-demo"
SCHEMA="$ROOT/examples/project_scaffold_demo.json"

if [[ ! -x "$DEMO_BIN" ]]; then
  echo "Building TUI demo..."
  cmake -S "$ROOT/tui" -B "$ROOT/tui/build"
  cmake --build "$ROOT/tui/build" --target configwizard-tui-demo
fi

exec "$DEMO_BIN" "$SCHEMA"
