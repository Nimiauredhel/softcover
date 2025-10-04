#include "app_entity.h"
#include "app_audio.h"
#include "app_memory.h"

EntityDefinition_t* entity_get_definition(uint16_t entity_idx)
{
    Entity_t *entity = &serializables->scenes[serializables->current_scene_index].entities[entity_idx];

    if (entity->definition_is_local)
    {
        return &serializables->scenes[serializables->current_scene_index].definitions[entity->definition_idx];
    }
    else
    {
        return &ephemerals->definitions[entity->definition_idx];
    }
}

Sprite_t* entity_get_sprite(uint16_t entity_idx)
{
    Entity_t *entity = &serializables->scenes[serializables->current_scene_index].entities[entity_idx];

    if (entity->definition_is_local)
    {
        return &serializables->scenes[serializables->current_scene_index].sprites[entity->definition_idx];
    }
    else
    {
        return &ephemerals->sprites[entity->definition_idx];
    }
}

Collider_t* entity_get_collider(uint16_t entity_idx)
{
    Entity_t *entity = &serializables->scenes[serializables->current_scene_index].entities[entity_idx];

    if (entity->definition_is_local)
    {
        return &serializables->scenes[serializables->current_scene_index].colliders[entity->definition_idx];
    }
    else
    {
        return &ephemerals->colliders[entity->definition_idx];
    }
}

SoundEmitter_t* entity_get_sounds(uint16_t entity_idx)
{
    Entity_t *entity = &serializables->scenes[serializables->current_scene_index].entities[entity_idx];

    if (entity->definition_is_local)
    {
        return &serializables->scenes[serializables->current_scene_index].sound_emitters[entity->definition_idx];
    }
    else
    {
        return &ephemerals->sound_emitters[entity->definition_idx];
    }
}

int32_t entity_create(int32_t scene_idx, uint16_t definition_idx, bool local_def, uint8_t layer, int32_t x, int32_t y)
{
    Scene_t *scene = serializables->scenes+scene_idx;
    uint16_t index = scene->entity_count;

    if (index >= SCENE_ENTITIES_MAX_COUNT)
    {
        platform->debug_log("Cannot add new entity, limit reached.");
        return -1;
    }

    scene->entity_count++;

    Entity_t *entity = &scene->entities[index];
    EntityDefinition_t *definition = local_def ? &scene->definitions[definition_idx] : &ephemerals->definitions[definition_idx];

    entity->used = true;
    entity->definition_is_local = local_def;
    entity->layer = layer;
    entity->definition_idx = definition_idx;

    entity->transform.x_pos = x;
    entity->transform.y_pos = y;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Created new %s[%u] at index %u, [%u,%u] layer %u.",
            definition->name, definition_idx, index, x, y, layer);
    platform->debug_log(ephemerals->debug_buff);

    return index;
}

void entities_initialize_draw_order(void)
{
    Scene_t *scene = serializables->scenes+serializables->current_scene_index;

    if (scene->entity_count <= 0)
    {
        platform->debug_log("Cannot initialize draw order - entity count is zero!");
        return;
    }

    /// initialize unsorted state
    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        ephemerals->entities_draw_order[i] = i;
    }

    /// initialize layer offsets to -1, e.g. no entities using that layer
    for (uint8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        ephemerals->entities_draw_order_layer_offsets[i] = -1;
    }

    entities_sort_by_comparer(0, scene->entity_count-1, entities_compare_layer);

    uint8_t layer = 0;
    Entity_t *entity;

    // default layer 0 offset before the actual work
    entity = scene->entities+ephemerals->entities_draw_order[0];
    layer = entity->layer;
    ephemerals->entities_draw_order_layer_offsets[entity->layer] = 0;

    for (uint16_t i = 1; i < scene->entity_count; i++)
    {
        entity = scene->entities+ephemerals->entities_draw_order[i];

        if (entity->layer > layer)
        {
            layer = entity->layer;
            ephemerals->entities_draw_order_layer_offsets[layer] = i;
        }
    }
}

void entity_set_pos(uint32_t entity_id, int32_t x, int32_t y)
{
    serializables->scenes[serializables->current_scene_index].entities[entity_id].transform.x_pos = x;
    serializables->scenes[serializables->current_scene_index].entities[entity_id].transform.y_pos = y;

    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Set Thing %u at position [%d,%d].", entity_id, x, y);
    platform->debug_log(ephemerals->debug_buff);
}

int8_t entities_compare_y(uint16_t first_id, uint16_t second_id)
{
    return (serializables->scenes[serializables->current_scene_index].entities[first_id].transform.y_pos
            + entity_get_sprite(first_id)->y_offset)
         - (serializables->scenes[serializables->current_scene_index].entities[second_id].transform.y_pos
            + entity_get_sprite(second_id)->y_offset);
}

int8_t entities_compare_layer(uint16_t first_id, uint16_t second_id)
{
    return serializables->scenes[serializables->current_scene_index].entities[first_id].layer
         - serializables->scenes[serializables->current_scene_index].entities[second_id].layer;
}

void entities_sort_by_comparer(uint32_t start_idx, uint32_t end_idx, int8_t (*comparer)(uint16_t, uint16_t))
{
    uint16_t temp;
    bool swapped = false;

    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    for (; end_idx > start_idx; end_idx--)
    {
        if (!scene->entities[ephemerals->entities_draw_order[end_idx]].used)
        {
            continue;
        }

        swapped = false;

        for (uint16_t i = start_idx; i < end_idx; i++)
        {
            if (comparer(
                        ephemerals->entities_draw_order[i],
                        ephemerals->entities_draw_order[i+1])
                        > 0)
            {
                swapped = true;
                temp = ephemerals->entities_draw_order[i];
                ephemerals->entities_draw_order[i] = ephemerals->entities_draw_order[i+1];
                ephemerals->entities_draw_order[i+1] = temp;
            }
        }

        if (!swapped) break;
    }
}

void entities_update_draw_order(void)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (scene->entity_count <= 0)
    {
        platform->debug_log("Cannot update draw order - entity count is zero!");
        return;
    }

    int32_t start_idx = 0;
    int32_t end_idx = 0;

    for (int8_t i = 0; i < APP_LAYER_COUNT; i++)
    {
        start_idx = ephemerals->entities_draw_order_layer_offsets[i];

        if (start_idx < 0) continue;

        end_idx = -1;

        for (int8_t j = i+1; j < APP_LAYER_COUNT; j++)
        {
            if (ephemerals->entities_draw_order_layer_offsets[j] > start_idx)
            {
                end_idx = ephemerals->entities_draw_order_layer_offsets[j] - 1;
                break;
            }
        }

        if (end_idx < 0)
        {
            entities_sort_by_comparer(start_idx, scene->entity_count - 1, entities_compare_y);
            break;
        }

        entities_sort_by_comparer(start_idx, end_idx, entities_compare_y);
    }
}

void entities_get_distance(uint16_t first_entity_id, uint16_t second_entity_id, int32_t *x_dist_out, int32_t *y_dist_out)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    *x_dist_out = scene->entities[first_entity_id].transform.x_pos - scene->entities[second_entity_id].transform.x_pos;
    *y_dist_out = scene->entities[first_entity_id].transform.y_pos - scene->entities[second_entity_id].transform.y_pos;

    if (*x_dist_out < 0) *x_dist_out *= -1;
    if (*y_dist_out < 0) *y_dist_out *= -1;
}

uint16_t entity_test_collision(uint16_t entity_id)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];
    Entity_t *entity = &scene->entities[entity_id];
    EntityDefinition_t *definition = entity->definition_is_local ? &scene->definitions[entity->definition_idx] : &ephemerals->definitions[entity->definition_idx];

    if (!(definition->flags & ENTITY_FLAGS_COLLISION)) return entity_id;

    Transform_t *this_transform = &scene->entities[entity_id].transform;
    Collider_t *this_collider = entity_get_collider(entity_id);

    Transform_t *other_transform = NULL;
    Collider_t *other_collider = NULL;

    int16_t this_left =   this_transform->x_pos + this_collider->min_x;
    int16_t this_right =  this_transform->x_pos + this_collider->max_x;
    int16_t this_top =    this_transform->y_pos + this_collider->min_y;
    int16_t this_bottom = this_transform->y_pos + this_collider->max_y;

    int16_t other_left = 0;
    int16_t other_right = 0;
    int16_t other_top = 0;
    int16_t other_bottom = 0;

    for (uint16_t i = 0; i < scene->entity_count; i++)
    {
        /// avoid testing against self
        if (i == entity_id) continue;
        /// avoid testing against non-collider
        if (!(entity_get_definition(i)->flags & ENTITY_FLAGS_COLLISION)) continue;

        other_transform = &scene->entities[i].transform;
        other_collider = entity_get_collider(i);

        other_left =   other_transform->x_pos + other_collider->min_x;
        other_right =  other_transform->x_pos + other_collider->max_x;
        other_top =    other_transform->y_pos + other_collider->min_y;
        other_bottom = other_transform->y_pos + other_collider->max_y;

        if(this_right >= other_left
        && this_left <= other_right
        && this_top <= other_bottom
        && this_bottom >= other_top)
        {
            snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d collided with Thing %d.", entity_id, i);
            platform->debug_log(ephemerals->debug_buff);
            return i;
        }
    }

    return entity_id;
}

void entity_move(uint16_t entity_id, int16_t x_delta, int16_t y_delta)
{
    Scene_t *scene = &serializables->scenes[serializables->current_scene_index];

    if (!scene->entities[entity_id].used) return;

    scene->entities[entity_id].transform.x_pos += x_delta;
    scene->entities[entity_id].transform.y_pos += y_delta;

    size_t sfx_offset = ephemerals->sound_offsets[entity_get_sounds(entity_id)->move_sfx_idx];
    audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+sfx_offset));

    uint16_t other_idx = entity_test_collision(entity_id);

    if (other_idx != entity_id)
    {
        /*
    COLL_FLAGS_NONE = 0x00,
    COLL_FLAGS_BLOCK = 0x01,
    COLL_FLAGS_PLAY_SOUND = 0x02,
    COLL_FLAGS_SET_SCENE = 0x04,
    COLL_FLAGS_SET_POSITION = 0x08,
    COLL_FLAGS_CALLBACK = 0x10,
    */
        Collider_t *other_coll = entity_get_collider(other_idx);

        if (other_coll->flags & COLL_FLAGS_BLOCK)
        {
            entity_set_pos(entity_id, scene->entities[entity_id].transform.x_pos - x_delta, scene->entities[entity_id].transform.y_pos - y_delta);
        }

        if (other_coll->flags & COLL_FLAGS_PLAY_SOUND)
        {
            audio_push_clip((AudioClip_t *)(ephemerals->bump_buffer+ephemerals->sound_offsets[other_coll->params[0]]));
        }

        if ((other_coll->flags & COLL_FLAGS_SET_SCENE)
            && entity_id == serializables->controlled_entity_idx)
        {
            load_scene_by_index(other_coll->params[1]);
            /// TODO: this is obviously a bad way to do it and needs to go later
            serializables->controlled_entity_idx = entity_id;
        }

        if (other_coll->flags & COLL_FLAGS_SET_POSITION)
        {
            entity_set_pos(entity_id, other_coll->params[2], other_coll->params[3]);
        }

        if (other_coll->flags & COLL_FLAGS_CALLBACK)
        {
            /// TODO: add collision callback array
        }

        return;
    }

    /*
    snprintf(ephemerals->debug_buff, sizeof(ephemerals->debug_buff), "Thing %d moved to [%d,%d]", entity_idx,
            serializables->entities[entity_idx].x_pos, serializables->entities[entity_idx].y_pos);
    platform->debug_log(ephemerals->debug_buff);
    */
}
