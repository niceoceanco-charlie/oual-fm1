/* A tiny string.h for the freestanding WebAssembly build (no libc): only what the engine and the codec encoder use. GPL-3.0-only. */
#ifndef OUAL_WASM_STRING_H
#define OUAL_WASM_STRING_H
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
#endif
