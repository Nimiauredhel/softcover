#ifndef SOFTCOVER_PLATFORM_H
#define SOFTCOVER_PLATFORM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common_structs.h"

#define DEBUG_MESSAGE_MAX_LEN (256)

typedef struct Platform Platform_t;
typedef struct PlatformCapabilities PlatformCapabilities_t;
typedef struct PlatformSettings PlatformSettings_t;

typedef struct AppMemoryPartition AppMemoryPartition_t;

typedef void (*AppSetupFunc)(const Platform_t *interface);
typedef void (*AppInitFunc)(const Platform_t *interface, AppMemoryPartition_t *memory);
typedef void (*AppLoopFunc)(void);
typedef void (*AppExitFunc)(void);

struct PlatformCapabilities
{
    size_t app_memory_max_bytes;

    size_t gfx_buffer_max_bytes;
    uint32_t gfx_buffer_width_max;
    uint32_t gfx_buffer_height_max;
    uint8_t gfx_pixel_max_bytes;

    uint32_t gfx_frame_time_min_us;

    uint8_t audio_channels_max;
    uint32_t audio_buffer_capacity_max;

    uint16_t input_buffer_capacity_max;
};

struct PlatformSettings
{
    size_t app_memory_serializable_bytes;
    size_t app_memory_ephemeral_bytes;

    uint32_t gfx_buffer_width;
    uint32_t gfx_buffer_height;
    uint8_t gfx_pixel_size_bytes;

    uint32_t gfx_frame_time_target_us;

    uint8_t audio_channels;
    uint32_t audio_buffer_capacity;

    uint16_t input_buffer_capacity;
};

struct Platform
{
    // configuration
    const PlatformCapabilities_t *capabilities;
    PlatformSettings_t *settings;
    // time
    int64_t (*time_get_delta_us)(void);
    // audio
    float (*audio_get_volume)(void);
    void (*audio_set_volume)(float);
    // storage
    bool (*gfx_load_texture)(char *name, Texture_t *dest, size_t max_size);
    bool (*audio_load_wav)(char *name, AudioClip_t *dest, size_t max_size);
    size_t (*storage_load_text)(const char *name, char *dest, size_t max_len);
    void (*storage_save_state)(char *state_name);
    void (*storage_load_state)(char *state_name);
    // utils
    bool (*get_should_terminate)(void);
    void (*set_should_terminate)(bool value);
    void (*debug_log)(char *message);
    void (*debug_break)(void);

};

struct AppMemoryPartition
{
    Memory_t *serializable;
    Memory_t *ephemeral;

    UniformRing_t *input_buffer;
    Texture_t *gfx_buffer;
    UniformRing_t *audio_buffer;
};

#endif
