/* A tiny stdlib.h for the freestanding WebAssembly build: calloc/free come from a bump arena (wasm/libc.c), abs is inline. GPL-3.0-only. */
#ifndef OUAL_WASM_STDLIB_H
#define OUAL_WASM_STDLIB_H
#include <stddef.h>
void *calloc(size_t n, size_t m);
void free(void *p);   /* does nothing: the arena is released in one go (oual_arena_release) */
static inline int abs(int x) { return x < 0 ? -x : x; }
#endif
