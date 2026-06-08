// memory_demo.hpp - simple arena to track malloc/free and measure fragmentation

#pragma once

static constexpr int ARENA_MAX_BLOCKS = 64;

// one entry per malloc'd block
struct MemBlock {
    void*              ptr;       // pointer returned by malloc
    unsigned long long size;      // size requested
    unsigned long long allocated; // actual size returned by malloc (may be rounded up)
    bool               in_use;    // true if currently allocated
};

// tracks all blocks and some basic stats
struct MemArena {
    MemBlock           blocks[ARENA_MAX_BLOCKS];
    int                count;            // blocks currently in use
    unsigned long long total_requested;  // sum of requested sizes
    unsigned long long total_allocated;  // sum of actual allocated sizes
    unsigned long long peak_usage;       // highest total_requested seen so far
    int                alloc_count;      // total allocs made
    int                free_count;       // total frees made
    void*              backing;          // backing memory (arena itself)
    unsigned long long backing_size;     // size of backing memory
};

// set up the arena with a given backing buffer size
void arena_init(MemArena* arena, unsigned long long backing_size);

// allocate from the arena, returns null if full or size is 0
void* arena_alloc(MemArena* arena, unsigned long long size);

// free a block, returns false if ptr wasnt found
bool arena_free(MemArena* arena, void* ptr);

// fragmentation as a percent: (allocated - requested) / allocated * 100
float arena_fragmentation(const MemArena* arena);

// write stats to buf, returns chars written
int arena_stats(const MemArena* arena, char* buf, int bufsize);

// frees all blocks and backing memory
void arena_destroy(MemArena* arena);
