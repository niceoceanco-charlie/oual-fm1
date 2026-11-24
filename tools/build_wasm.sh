#!/bin/sh
# Builds the oual engine as WebAssembly with plain clang + wasm-ld (no Emscripten, no libc). From the repo root:
#   tools/build_wasm.sh [outdir]      needs clang with the wasm32 target, wasm-ld (package lld), and node (to check the result)
# Writes outdir/{oual.wasm, oual-core.mjs, oual.mjs, oual-worklet.js, manifest.json, SHA256SUMS}. GPL-3.0-only.
set -eu
OUT=${1:-out/wasm}; CLANG=${CLANG:-clang}; WASMLD=${WASMLD:-wasm-ld}
VERSION=$(cat wasm/VERSION); COMMIT=${GITHUB_SHA:-$(git rev-parse HEAD 2>/dev/null || echo unknown)}
mkdir -p "$OUT/obj"
CF="--target=wasm32 -O2 -ffreestanding -nostdlib -Iwasm/include -Wall -Wextra -Wno-unused-function -Wno-unused-parameter"
$CLANG $CF -fno-builtin -c wasm/libc.c -o "$OUT/obj/libc.o"
$CLANG $CF -c core/oual_engine.c -o "$OUT/obj/oual_engine.o"
$CLANG $CF -c wasm/oual_wasm.c -o "$OUT/obj/oual_wasm.o"
$WASMLD --no-entry --gc-sections --strip-all -z stack-size=65536 --initial-memory=2097152 --max-memory=268435456 \
  "$OUT/obj/oual_wasm.o" "$OUT/obj/oual_engine.o" "$OUT/obj/libc.o" -o "$OUT/oual.wasm"
cp web/oual-wasm/oual-core.mjs web/oual-wasm/oual.mjs "$OUT/"
# the worklet is a classic script: the core without its `export` keywords, then the processor
{ sed 's/^export //' web/oual-wasm/oual-core.mjs; cat web/oual-wasm/oual-worklet.part.js; } > "$OUT/oual-worklet.js"
node --check "$OUT/oual-worklet.js"
node tools/wasm_manifest.mjs "$OUT" "$VERSION" "$COMMIT"
