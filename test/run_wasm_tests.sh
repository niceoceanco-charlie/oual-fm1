#!/bin/sh
# WebAssembly tests: renders test/wasm/patch_*.txt with the native engine (tools/oual_host) and the same scripts through the built module.
# usage: test/run_wasm_tests.sh <wasm dir>   (from the repo root; needs cc and node)
set -eu
W=${1:-out/wasm}
./tools/make_test_pack.sh >/dev/null
cc -O2 -o /tmp/oual_host_native tools/oual_host.c core/oual_engine.c -lm
mkdir -p out/wasm_native
for p in patch_a patch_b; do /tmp/oual_host_native test/wasm/$p.txt out/wasm_native/$p.wav 4 >/dev/null; done
node test/test_wasm.mjs "$W" out/wasm_native
