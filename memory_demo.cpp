// memory_demo.cpp
// Implementation of the memory arena.

#include "memory_demo.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace std;

void arena_init(MemArena* arena, unsigned long long backing_size) {
    memset(arena, 0, sizeof(*arena));
    arena->backing_size = backing_size;
    arena->backing = malloc(backing_size);
}

void* arena_alloc(MemArena* arena, unsigned long long size) {
    if (size == 0 || arena->count >= ARENA_MAX_BLOCKS) return nullptr;

    void* ptr = malloc(size);
    if (!ptr) return nullptr;

    MemBlock* blk = &arena->blocks[arena->count];
    blk->ptr       = ptr;
    blk->size      = size;
    blk->allocated = size;  // in practice, malloc may round up; shown for demo
    blk->in_use    = true;

    arena->count++;
    arena->total_requested += size;
    arena->total_allocated  += size;
    arena->alloc_count++;

    if (arena->total_requested > arena->peak_usage)
        arena->peak_usage = arena->total_requested;

    return ptr;
}

bool arena_free(MemArena* arena, void* ptr) {
    for (int i = 0; i < arena->count; ++i) {
        if (arena->blocks[i].ptr == ptr && arena->blocks[i].in_use) {
            arena->blocks[i].in_use = false;
            arena->total_requested -= arena->blocks[i].size;
            arena->total_allocated  -= arena->blocks[i].allocated;
            arena->free_count++;
            free(ptr);

            // Compact: move last block into this slot
            if (i < arena->count - 1) {
                arena->blocks[i] = arena->blocks[arena->count - 1];
            }
            arena->count--;
            return true;
        }
    }
    return false;
}

float arena_fragmentation(const MemArena* arena) {
    if (arena->total_allocated == 0) return 0.0f;
    return (float)(arena->total_allocated - arena->total_requested)
           / (float)arena->total_allocated * 100.0f;
}

int arena_stats(const MemArena* arena, char* buf, int bufsize) {
    float frag = arena_fragmentation(arena);
    return snprintf(buf, bufsize,
        "Arena: %d/%d blocks used | %.1f%% fragmentation | "
        "peak: %llu bytes | allocs: %d frees: %d",
        arena->count, ARENA_MAX_BLOCKS, (double)frag,
        arena->peak_usage, arena->alloc_count, arena->free_count);
}

void arena_destroy(MemArena* arena) {
    for (int i = 0; i < arena->count; ++i) {
        if (arena->blocks[i].in_use && arena->blocks[i].ptr) {
            free(arena->blocks[i].ptr);
        }
    }
    if (arena->backing) free(arena->backing);
    memset(arena, 0, sizeof(*arena));
}