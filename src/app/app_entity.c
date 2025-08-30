#include "app_entity.h"
#include "app_ephemeral_state.h"

EntityDefinition_t* thing_get_definition(EntityLive_t *thing)
{
    return &ephemerals->definitions[thing->definition_idx];
}

Sprite_t* entity_get_sprite(EntityLive_t *thing)
{
    return &ephemerals->sprites[thing->definition_idx];
}

Collider_t* thing_get_collider(EntityLive_t *thing)
{
    return &ephemerals->colliders[thing->definition_idx];
}

SoundEmitter_t* entity_get_sounds(EntityLive_t *thing)
{
    return &ephemerals->sound_emitters[thing->definition_idx];
}
