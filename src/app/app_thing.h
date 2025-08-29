#ifndef APP_THING_H
#define APP_THING_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

typedef struct Prefab
{
    ThingFlags_t flags;
} Prefab_t;

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

typedef struct Thing
{
    bool used;
    uint8_t layer;
    uint16_t prefab_idx;
    Transform_t transform;
} Thing_t;

#endif

