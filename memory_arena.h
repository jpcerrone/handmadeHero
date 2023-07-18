#pragma once
#include <cinttypes>

struct MemoryArena {
    uint8_t* base;
    size_t used;
    size_t total;
};