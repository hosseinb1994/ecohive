#!/usr/bin/env bash
# Build the Release firmware with one fault-classifier model active and flash it.
#
# Usage: ./deploy_model.sh TREE|LOGREG|MLP
#
# Rewrites Core/Inc/bench_config.h so exactly the requested MODEL_* is
# uncommented, clean-rebuilds the Release preset, then flashes over SWD via
# STM32_Programmer_CLI. Repo is left with the requested model active.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

BENCH_CONFIG="Core/Inc/bench_config.h"
NINJA_LINK="$HOME/.local/bin/ninja"
PROGRAMMER_LINK="$HOME/.local/bin/STM32_Programmer_CLI"

usage() {
    echo "Usage: $0 TREE|LOGREG|MLP" >&2
    exit 1
}

[ $# -eq 1 ] || usage
case "$1" in
    TREE)   MODEL=MODEL_TREE ;;
    LOGREG) MODEL=MODEL_LOGREG ;;
    MLP)    MODEL=MODEL_MLP ;;
    *)      usage ;;
esac

# --- 1. Make sure the bundled ninja symlink is valid; re-resolve via snap's
#        own "current" pointer if the extension/toolchain got updated and the
#        old target vanished. ---
if [ ! -x "$NINJA_LINK" ]; then
    echo "==> Stable ninja symlink missing/stale, re-resolving..."
    mkdir -p "$(dirname "$NINJA_LINK")"
    NINJA_REAL="$(find -L "$HOME/snap/code/current" -path "*/stm32cube/bundles/ninja/*/bin/ninja" -type f 2>/dev/null | sort -V | tail -1)"
    if [ -z "$NINJA_REAL" ]; then
        echo "ERROR: could not locate a bundled ninja binary under $HOME/snap/code/current" >&2
        exit 1
    fi
    ln -sf "$NINJA_REAL" "$NINJA_LINK"
    echo "==> Relinked $NINJA_LINK -> $NINJA_REAL"
fi

# Same self-healing pattern as ninja: resolve through snap's "current"
# pointer so the STM32Cube Programmer CLI also survives snap revision bumps.
if [ ! -x "$PROGRAMMER_LINK" ]; then
    echo "==> Stable STM32_Programmer_CLI symlink missing/stale, re-resolving..."
    mkdir -p "$(dirname "$PROGRAMMER_LINK")"
    PROGRAMMER_REAL="$(find -L "$HOME/snap/code/current" -path "*/stm32cube/bundles/programmer/*/bin/STM32_Programmer_CLI" -type f 2>/dev/null | sort -V | tail -1)"
    if [ -z "$PROGRAMMER_REAL" ]; then
        echo "ERROR: could not locate STM32_Programmer_CLI under $HOME/snap/code/current" >&2
        exit 1
    fi
    ln -sf "$PROGRAMMER_REAL" "$PROGRAMMER_LINK"
    echo "==> Relinked $PROGRAMMER_LINK -> $PROGRAMMER_REAL"
fi

# --- 2. Set the requested model as the only one active in bench_config.h ---
echo "==> Setting active model: $MODEL"
for m in MODEL_TREE MODEL_LOGREG MODEL_MLP; do
    if [ "$m" = "$MODEL" ]; then
        sed -i -E "s|^/\* *#define $m *\*/|#define $m|" "$BENCH_CONFIG"
    else
        sed -i -E "s|^#define $m\$|/* #define $m */|" "$BENCH_CONFIG"
    fi
done
grep -n "^#define MODEL\|^/\* #define MODEL" "$BENCH_CONFIG"

# --- 3. Clean-rebuild Release ---
echo "==> Configuring Release preset"
rm -rf build/Release
cmake --preset Release

echo "==> Building Release"
cmake --build build/Release

echo "==> Size report"
arm-none-eabi-size build/Release/Ecohive.elf

# --- 4. Flash over SWD ---
echo "==> Flashing $MODEL"
"$PROGRAMMER_LINK" -c port=SWD -w build/Release/Ecohive.elf -v -rst

echo "==> Done. $MODEL is running - read the #BENCH line from UART now."
