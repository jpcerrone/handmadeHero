#pragma once
#include <cinttypes>
#include "memory_arena.h"
#define pushStruct(arena, type) (type*)pushSize(arena, sizeof(type)) // the (type*) performs a cast on the return value of pushSize
#define pushArray(arena, count, type) (type*)pushSize(arena, sizeof(type)*(count))
void* pushSize(MemoryArena* arena, size_t sizeToPush) {
    //Assert(arena->used + sizeToPush < arena->total);
    void* pointerToStruct = arena->base + arena->used;
    arena->used += sizeToPush;
    return pointerToStruct;
}
