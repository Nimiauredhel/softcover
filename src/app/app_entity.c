#include "app_entity.h"
#include "app_ephemeral_state.h"

EntityDefinition_t* entity_get_definition(EntityLive_t *entity)
{
    return &ephemerals->definitions[entity->definition_idx];
}

Sprite_t* entity_get_sprite(EntityLive_t *entity)
{
    return &ephemerals->sprites[entity->definition_idx];
}

Collider_t* entity_get_collider(EntityLive_t *entity)
{
    return &ephemerals->colliders[entity->definition_idx];
}

SoundEmitter_t* entity_get_sounds(EntityLive_t *entity)
{
    return &ephemerals->sound_emitters[entity->definition_idx];
}
