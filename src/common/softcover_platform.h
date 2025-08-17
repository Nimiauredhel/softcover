#ifndef SOFTCOVER_PLATFORM_H
#define SOFTCOVER_PLATFORM_H

#include <stdlib.h>
#include <stdint.h>

typedef struct Platform Platform_t;
typedef struct Memory Memory_t;

typedef void (*AppInitFunc)(Platform_t *platform, Memory_t **memory_pptr);
typedef void (*AppLoopFunc)(Platform_t *platform, Memory_t *memory);

struct Platform
{
    // memory
    Memory_t* (*memory_allocate)(size_t size);
    void (*memory_release)(Memory_t *memory);
    // gfx
    void (*gfx_clear_buffer)(void);
    int (*gfx_load_texture)(char *name);
    void (*gfx_draw_texture)(int idx, int x, int y);
    // input
    volatile char (*input_read)(void);

};

struct Memory
{
    size_t size;
    uint8_t buffer[];
};

#endif
