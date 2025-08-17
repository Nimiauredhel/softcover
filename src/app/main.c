#include <stdio.h>
#include <stdint.h>

#include "softcover_platform.h"

typedef struct Thing
{
    uint8_t texture_id;
    int8_t x;
    int8_t y;
} Thing_t;

void app_init(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len)
{
    Thing_t *player_thing = ((Thing_t *)state_mem+0);
    Thing_t *other_thing = ((Thing_t *)state_mem+3);

    player_thing->texture_id = platform->gfx_load_texture("player thing");
    player_thing->x = 0;
    player_thing->y = 0;

    other_thing->texture_id = platform->gfx_load_texture("other thing");
    other_thing->x = 5;
    other_thing->y = 5;
}

void app_loop(Platform_t *platform, uint8_t *state_mem, size_t state_mem_len)
{
    static const int8_t mov_speed = 5;

    Thing_t *player_thing = ((Thing_t *)state_mem+0);
    Thing_t *other_thing = ((Thing_t *)state_mem+3);

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            player_thing->y -= mov_speed;
            break;
        case 'a':
            player_thing->x -= mov_speed;
            break;
        case 's':
            player_thing->y += mov_speed;
            break;
        case 'd':
            player_thing->x += mov_speed;
            break;
        default:
            break;
    }

    platform->gfx_clear_buffer();
    platform->gfx_draw_texture(player_thing->texture_id, player_thing->x, player_thing->y);
    platform->gfx_draw_texture(other_thing->texture_id, other_thing->x, other_thing->y);
}
