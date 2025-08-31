#include "app_gfx.h"

#include <stdbool.h>
#include <string.h>

#include "app_memory.h"
#include "app_scene.h"
#include "app_entity.h"
#include "app_serializable_state.h"
#include "app_ephemeral_state.h"

bool debug_gfx = false;

void gfx_world_to_screen_coords(int16_t *x_ptr, int16_t *y_ptr)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    *x_ptr += gfx_buffer->width/2;
    *x_ptr -= scene->entities[serializables->focal_entity_idx].transform.x_pos;
    *y_ptr += gfx_buffer->height/2;
    *y_ptr -= scene->entities[serializables->focal_entity_idx].transform.y_pos;
}

void gfx_clear_buffer(void)
{
    if (gfx_buffer == NULL) return;
    memset(gfx_buffer->pixels, 0,
    gfx_buffer->width * gfx_buffer->height * gfx_buffer->pixel_size_bytes);
}

void gfx_draw_texture(Texture_t *texture, int start_x, int start_y)
{
    if (gfx_buffer == NULL) return;

    int pixel_x;
    int pixel_y;

    for (uint16_t texture_y = 0; texture_y < texture->height; texture_y++)
    {
        pixel_y = start_y + texture_y;
        if (pixel_y >= gfx_buffer->height || pixel_y < 0) continue;

        for (uint16_t texture_x = 0; texture_x < texture->width; texture_x++)
        {
            pixel_x = start_x + texture_x;
            if (pixel_x >= gfx_buffer->width  || pixel_x < 0) continue;

            uint16_t texture_idx = texture->pixel_size_bytes * (texture_x + (texture_y * texture->width));
            uint16_t buffer_idx = gfx_buffer->pixel_size_bytes * (pixel_x + (pixel_y * gfx_buffer->width));

            /// extremely dumb alpha test
            /// TODO: somehow move this responsibility to the platform side
            if ((texture->pixel_size_bytes == 1 && texture->pixels[texture_idx] > 64)
             || (texture->pixel_size_bytes == 4 && texture->pixels[texture_idx+3] == 0))
            {
                continue;
            }

            for (uint8_t i = 0; i < gfx_buffer->pixel_size_bytes; i++)
            {
                gfx_buffer->pixels[buffer_idx+i] = texture->pixels[texture_idx+i];
            }
        }
    }
}

void gfx_debug_draw_collider(uint32_t thing_idx, uint32_t draw_val)
{
    if (gfx_buffer == NULL) return;

    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (!scene->entities[thing_idx].used) return;
    if (!(ephemerals->definitions[scene->entities[thing_idx].definition_idx].flags & THING_FLAGS_COLLISION)) return;

    Transform_t *transform = &scene->entities[thing_idx].transform;
    Collider_t *collider = &ephemerals->colliders[scene->entities[thing_idx].definition_idx];

    int16_t min_x = collider->min_x + transform->x_pos;
    int16_t min_y = collider->min_y + transform->y_pos;
    int16_t max_x = collider->max_x + transform->x_pos;
    int16_t max_y = collider->max_y + transform->y_pos;

    gfx_world_to_screen_coords(&min_x, &min_y);
    gfx_world_to_screen_coords(&max_x, &max_y);

    for (int16_t y = min_y; y <= max_y; y++)
    {
        if (y == min_y || y == max_y)
        {
            for (int16_t x = min_x; x <= max_x; x++)
            {
                if (x < 0 || x >= gfx_buffer->width || y < 0 || y >= gfx_buffer->height) continue;
                uint16_t buffer_idx = gfx_buffer->pixel_size_bytes * (x + (y * gfx_buffer->width));

                for (int i = 0; i < gfx_buffer->pixel_size_bytes; i++)
                {
                    gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }
        }
        else
        {
            if (min_x >= 0 && min_x < gfx_buffer->width && y >= 0 && y < gfx_buffer->height)
            {
                uint16_t buffer_idx = gfx_buffer->pixel_size_bytes * (min_x + (y * gfx_buffer->width));

                for (int i = 0; i < gfx_buffer->pixel_size_bytes; i++)
                {
                    gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }

            if (max_x >= 0 && max_x < gfx_buffer->width && y >= 0 && y < gfx_buffer->height)
            {
                uint16_t buffer_idx = gfx_buffer->pixel_size_bytes * (max_x + (y * gfx_buffer->width));

                for (int i = 0; i < gfx_buffer->pixel_size_bytes; i++)
                {
                    gfx_buffer->pixels[buffer_idx+i] = draw_val;
                }
            }
        }
    }
}

void gfx_draw_thing(uint32_t thing_idx)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (!scene->entities[thing_idx].used) return;

    size_t texture_offset = ephemerals->texture_offsets[entity_get_sprite(thing_idx)->texture_idx];
    Texture_t *texture_ptr = (Texture_t *)(ephemerals->bump_buffer+texture_offset);

    int16_t x = scene->entities[thing_idx].transform.x_pos - entity_get_sprite(thing_idx)->x_offset;
    int16_t y = scene->entities[thing_idx].transform.y_pos - entity_get_sprite(thing_idx)->y_offset;

    gfx_world_to_screen_coords(&x, &y);

    gfx_draw_texture(texture_ptr, x, y);
}

void gfx_draw_all_entities_debug(void)
{
    static const uint16_t counter_thresh = 2;
    static uint16_t step = 0;
    static uint16_t counter = 0;

    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff),
            "Thing %u [%u,%u] layer %u.", ephemerals->entities_draw_order[step],
            step_thing->transform.x_pos, step_thing->transform.y_pos, step_thing->layer);
    platform->debug_log(ephemerals->debug_buff);
    */

    for (uint16_t i = 0; i < step; i++)
    {
        gfx_draw_thing(ephemerals->entities_draw_order[i]);
    }

    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        gfx_debug_draw_collider(ephemerals->entities_draw_order[i], 2);
    }

    if (counter >= counter_thresh)
    {
        counter = 0;
        step++;
    }
    else
    {
        counter++;
    }

    if (step >= scene->entity_count)
    {
        step = 0;
        debug_gfx = false;
    }
}

void gfx_draw_all_entities(void)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        gfx_draw_thing(ephemerals->entities_draw_order[i]);
    }
}
