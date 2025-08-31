#include "app_entity.h"
#include "app_serializable_state.h"
#include "app_ephemeral_state.h"

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
