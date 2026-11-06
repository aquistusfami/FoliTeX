#!/usr/bin/env bash
set -euo pipefail
FOLITEX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FOLITEX_EXE="$FOLITEX_ROOT/build/folitex"
FOLITEX_REBUILD=0
if [[ ! -x "$FOLITEX_EXE" ]]; then
    FOLITEX_REBUILD=1
else
    for FOLITEX_SOURCE in "$FOLITEX_ROOT"/src/*.cpp "$FOLITEX_ROOT"/src/*.h "$FOLITEX_ROOT/CMakeLists.txt"; do
        if [[ "$FOLITEX_SOURCE" -nt "$FOLITEX_EXE" ]]; then FOLITEX_REBUILD=1; break; fi
    done
fi
if [[ "$FOLITEX_REBUILD" == 1 ]]; then
    cmake -S "$FOLITEX_ROOT" -B "$FOLITEX_ROOT/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build "$FOLITEX_ROOT/build" --parallel "${FOLITEX_BUILD_JOBS:-2}"
    FOLITEX_EXE="$FOLITEX_ROOT/build/folitex"
fi
if [[ "${XDG_SESSION_TYPE:-}" == wayland && -n "${WAYLAND_DISPLAY:-}" ]]; then
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland;xcb}"
fi
exec "$FOLITEX_EXE" "$@"
