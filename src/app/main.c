#include <stdio.h>
#include <stdint.h>

#include "softcover_platform.h"

typedef struct Thing
{
    uint8_t texture_id;
    int8_t x;
    int8_t y;
} Thing_t;

typedef struct AppState
{
    Thing_t first_thing;
    Thing_t second_thing;
    Thing_t *controlled_thing;
} AppState_t;

/// must match prototype @ref AppInitFunc
void app_init(Platform_t *platform, Memory_t **memory_pptr)
{
    Memory_t *memory = platform->memory_allocate(4096);
    *memory_pptr = memory;

    AppState_t *state = (AppState_t *)memory->buffer;

    state->first_thing.texture_id = platform->gfx_load_texture("first thing");
    state->first_thing.x = 0;
    state->first_thing.y = 0;

    state->second_thing.texture_id = platform->gfx_load_texture("second thing");
    state->second_thing.x = 5;
    state->second_thing.y = 5;

    state->controlled_thing = &state->first_thing;
}

/// must match prototype @ref AppLoopFunc
void app_loop(Platform_t *platform, Memory_t *memory)
{
    static const int8_t mov_speed = 1;

    AppState_t *state = (AppState_t *)memory->buffer;

    char input = platform->input_read();

    switch (input)
    {
        case 'w':
            state->controlled_thing->y -= mov_speed;
            break;
        case 'a':
            state->controlled_thing->x -= mov_speed;
            break;
        case 's':
            state->controlled_thing->y += mov_speed;
            break;
        case 'd':
            state->controlled_thing->x += mov_speed;
            break;
        case 'q':
            state->controlled_thing =
                state->controlled_thing == &state->first_thing
                ? &state->second_thing
                : &state->first_thing;
            break;
        default:
            break;
    }

    platform->gfx_clear_buffer();
    platform->gfx_draw_texture(state->first_thing.texture_id, state->first_thing.x, state->first_thing.y);
    platform->gfx_draw_texture(state->second_thing.texture_id, state->second_thing.x, state->second_thing.y);
}
