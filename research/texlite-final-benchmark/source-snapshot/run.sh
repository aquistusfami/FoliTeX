#!/usr/bin/env bash
set -euo pipefail
TEXLITE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmake -S "$TEXLITE_ROOT" -B "$TEXLITE_ROOT/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$TEXLITE_ROOT/build" --parallel "${TEXLITE_BUILD_JOBS:-2}"
if [[ "${XDG_SESSION_TYPE:-}" == wayland && -n "${WAYLAND_DISPLAY:-}" ]]; then
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland;xcb}"
fi
exec "$TEXLITE_ROOT/build/texlite" "$@"
