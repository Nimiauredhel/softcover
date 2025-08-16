#include <stdio.h>
#include <stdint.h>

#include "softcover_platform.h"

void app_init(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len)
{
    printf("app init.\n");

    state_mem[0] = platform->gfx_load_texture("thing");
    state_mem[1] = platform->gfx_load_texture("other thing");
}

void app_loop(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len)
{
    printf("app loop.\n");

    platform->input_read();
    platform->gfx_clear_buffer();
    platform->gfx_draw_texture(state_mem[0], 12, 34);
    platform->gfx_draw_texture(state_mem[1], 4, 5);
}
