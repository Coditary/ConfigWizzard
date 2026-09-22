#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cargo run -p configwizard-ui-cli --bin configwizard-demo -- \
  --non-interactive \
  --answers "$ROOT/examples/demo-answers.json" \
  "$@"
