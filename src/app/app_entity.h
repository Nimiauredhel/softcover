#ifndef APP_THING_H
#define APP_THING_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define ENTITY_DEF_NAME_MAX_LEN (16)

typedef enum ThingFlags
{
    THING_FLAGS_NONE = 0x00,
    THING_FLAGS_TRANSFORM = 0x01,
    THING_FLAGS_TEXTURE = 0x02,
    THING_FLAGS_COLLISION = 0x04,
    THING_FLAGS_MOVEMENT = 0x08,
    THING_FLAGS_CONTROL = 0x10,
    THING_FLAGS_SOUND = 0x20,
} ThingFlags_t;

typedef enum CollisionFlags
{
    COLL_FLAGS_NONE = 0x00,
    COLL_FLAGS_BLOCK = 0x01,
    COLL_FLAGS_PLAY_SOUND = 0x02,
    COLL_FLAGS_SET_SCENE = 0x04,
    COLL_FLAGS_SET_POSITION = 0x08,
    COLL_FLAGS_CALLBACK = 0x10,
} CollisionFlags_t;

typedef struct EntityDefinition
{
    char name[ENTITY_DEF_NAME_MAX_LEN];
    ThingFlags_t flags;
} EntityDefinition_t;

typedef struct Sprite
{
    size_t texture_idx;
    int16_t x_offset;
    int16_t y_offset;
} Sprite_t;

typedef struct Collider
{
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
    CollisionFlags_t flags;
    /// will hold sound idx, scene idx, position x, position y, callback index,
    /// until morale/architecture improves
    uint16_t params[5];
} Collider_t;

typedef struct SoundEmitter
{
    size_t move_sfx_idx;
} SoundEmitter_t;

typedef struct Transform
{
    int16_t x_pos;
    int16_t y_pos;
} Transform_t;

typedef struct MoveTriggerLocal
{
    bool relative;
    int16_t x_pos;
    int16_t y_pos;
} MoveTriggerLocal_t;

typedef struct MoveTriggerScene
{
    bool relative;
    int32_t index;
} MoveTriggerScene_t;

typedef struct EntityStored
{
    char definition_name[ENTITY_DEF_NAME_MAX_LEN];
    uint8_t layer;
    Transform_t transform;
} EntityStored_t;

typedef struct EntityLive
{
    bool used;
    uint16_t definition_idx;
    uint8_t layer;
    Transform_t transform;
} EntityLive_t;

EntityDefinition_t* entity_get_definition(EntityLive_t *entity);
Sprite_t* entity_get_sprite(EntityLive_t *entity);
Collider_t* entity_get_collider(EntityLive_t *entity);
SoundEmitter_t* entity_get_sounds(EntityLive_t *entity);

#endif

