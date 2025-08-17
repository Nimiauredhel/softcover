#ifndef SOFTCOVER_PLATFORM_H
#define SOFTCOVER_PLATFORM_H

#include <stdlib.h>
#include <stdint.h>

typedef struct Platform Platform_t;

struct Platform
{
    // gfx
    void (*gfx_clear_buffer)(void);
    int (*gfx_load_texture)(char *name);
    void (*gfx_draw_texture)(int idx, int x, int y);
    // input
    volatile char (*input_read)(void);

};

#endif
