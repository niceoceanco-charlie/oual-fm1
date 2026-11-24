/* The few libc functions the engine and the encoder need, for the freestanding WebAssembly build. GPL-3.0-only.
 * Compile this file with -fno-builtin so the compiler does not turn these loops back into calls to themselves.
 * The arena is a bump allocator that starts at __heap_base and grows the linear memory when it must. */
#include <stddef.h>
#include <stdint.h>
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) {
  unsigned char *a = d; const unsigned char *b = s;
  if (a < b) { while (n--) *a++ = *b++; } else { a += n; b += n; while (n--) *--a = *--b; }
  return d; }
void *memset(void *d, int c, size_t n) { unsigned char *a = d; while (n--) *a++ = (unsigned char)c; return d; }

extern unsigned char __heap_base;
#define ARENA_MAX (96u * 1024u * 1024u)       /* the arena never grows past 96 MB */
static uint32_t arena_top;                     /* offset from __heap_base, 0 = not started */
uint32_t oual_arena_mark(void) { return arena_top; }
void oual_arena_release(uint32_t mark) { arena_top = mark; }
void *oual_arena_alloc(uint32_t n) {          /* zeroed, 16-byte aligned, 0 if it does not fit */
  uint32_t off = (arena_top + 15u) & ~15u;
  if (n > ARENA_MAX || off > ARENA_MAX - n) return 0;
  uint32_t end = (uint32_t)(uintptr_t)&__heap_base + off + n, have = (uint32_t)__builtin_wasm_memory_size(0) * 65536u;
  if (end < off || end > have) {               /* grow the memory by whole 64 KB pages */
    uint32_t need = (end - have + 65535u) / 65536u;
    if (end < have || __builtin_wasm_memory_grow(0, need) == (size_t)-1) return 0; }
  unsigned char *p = &__heap_base + off; memset(p, 0, n); arena_top = off + n; return p; }
void *calloc(size_t n, size_t m) { if (m && n > 0xffffffffu / m) return 0; return oual_arena_alloc((uint32_t)(n * m)); }
void free(void *p) { (void)p; }
