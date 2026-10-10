#!/usr/bin/env bash
# Windows x64 assets-only experiment; ARM64 Linux is the Proton host ABI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/native/mvgl-assets"
OUT="$ROOT/dist/mvgl-native"
mkdir -p "$OUT"
CC="${CC:-x86_64-w64-mingw32-gcc}"
HOOK=("$SRC/vendor/minhook/src/buffer.c" "$SRC/vendor/minhook/src/hook.c" "$SRC/vendor/minhook/src/trampoline.c" "$SRC/vendor/minhook/src/hde/hde64.c")
COMMON=(-O2 -Wall -Wextra -Werror -I"$SRC/vendor/minhook/include" -static-libgcc)
"$CC" "${COMMON[@]}" -shared "$SRC/assets.c" "$SRC/mvgl-asi.c" "${HOOK[@]}" -lbcrypt -o "$OUT/bepis-mvgl.asi"
"$CC" "${COMMON[@]}" "$SRC/assets.c" "$SRC/test-hooks.c" "${HOOK[@]}" -lbcrypt -o "$OUT/test-hooks.exe"
cp "$SRC/vendor/minhook/LICENSE.txt" "$OUT/MINHOOK-LICENSE.txt"
cp "$SRC/DSTS-SIGNATURES-LICENSE.txt" "$OUT/DSTS-SIGNATURES-LICENSE.txt"
cp "$ROOT/LICENSE" "$OUT/LICENSE"
