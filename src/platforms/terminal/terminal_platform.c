#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include "common_interface.h"
#include "common_structs.h"

#include "terminal_time.h"
#include "terminal_utils.h"
#include "terminal_ncurses.h"
#include "terminal_portaudio.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

Memory_t* memory_allocate(size_t size);
void memory_release(Memory_t **memory_pptr);
void storage_save_state(char *state_name);
void storage_load_state(char *state_name);
bool get_should_terminate(void);
void set_should_terminate(bool value);

static const char *state_filename_format = "%s.state";

static char lib_path[256] = LIB_NAME;

static char platform_top_debug_buff[256];

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static const char *app_setup_name = "app_setup";
static const char *app_init_name = "app_init";
static const char *app_loop_name = "app_loop";
static const char *app_exit_name = "app_exit";

static AppSetupFunc app_setup;
static AppInitFunc app_init;
static AppLoopFunc app_loop;
static AppExitFunc app_exit;

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

static AppMemoryPartition_t app_memory = {0};

static const PlatformCapabilities_t capabilities =
{
    .app_memory_max_bytes = 4096*1024,

    .gfx_buffer_max_bytes = 15360,
    .gfx_buffer_width_max = 160,
    .gfx_buffer_height_max = 32,
    .gfx_pixel_max_bytes = 1,

    .gfx_frame_time_min_us = 8333,

    .audio_buffer_capacity_max = 65534,

    .input_buffer_capacity_max = 1024,
};

static const PlatformSettings_t default_settings =
{
    .app_memory_serializable_bytes = capabilities.app_memory_max_bytes/10,
    .app_memory_ephemeral_bytes = capabilities.app_memory_max_bytes/10,

    .gfx_pixel_size_bytes = capabilities.gfx_pixel_max_bytes,
    .gfx_buffer_width = capabilities.gfx_buffer_width_max,
    .gfx_buffer_height = capabilities.gfx_buffer_height_max,

    .gfx_frame_time_target_us = capabilities.gfx_frame_time_min_us*2,

    .audio_buffer_capacity = 16384,

    .input_buffer_capacity = 128,
};

static PlatformSettings_t platform_settings = default_settings;

static const Platform_t platform =
{
    .capabilities = &capabilities,
    .settings = &platform_settings,

    /// time
    .time_get_delta_us = time_get_delta_us,

    /// audio
    .audio_get_volume = audio_get_volume,
    .audio_set_volume = audio_set_volume,

    /// storage
    .gfx_load_texture = gfx_load_texture,
    .audio_load_wav = audio_load_wav,
    .storage_save_state = storage_save_state,
    .storage_load_state = storage_load_state,

    /// utils
    .get_should_terminate = get_should_terminate,
    .set_should_terminate = set_should_terminate,
    .debug_log = debug_log,
    .debug_break = debug_break,
};

Memory_t* memory_allocate(size_t size)
{
    Memory_t *memory = (Memory_t *)malloc(size + sizeof(Memory_t));

    if (memory == NULL)
    {
        should_terminate = true;
        debug_log("Failed to allocate memory chunk.");
        return NULL;
    }

    explicit_bzero(memory->buffer, size);
    memory->size_bytes = size;
    return memory;
}

void memory_release(Memory_t **memory_pptr)
{
    if (*memory_pptr != NULL)
    {
        free(*memory_pptr);
        *memory_pptr = NULL;
    }
}

void storage_save_state(char *state_name)
{
    char file_name[256] = {0};
    FILE *file = NULL;

    snprintf(file_name, sizeof(file_name), state_filename_format, state_name);

    snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Saving '%s'.", file_name);
    debug_log(platform_top_debug_buff);

    file = fopen(file_name, "wb");

    if (file == NULL)
    {
        int err = errno;
        snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Failed to save '%s': %s", file_name, strerror(err));
        debug_log(platform_top_debug_buff);
        return;
    }
    
    fwrite(app_memory.serializable->buffer, 1, app_memory.serializable->size_bytes, file);
    fclose(file);
}

void storage_load_state(char *state_name)
{
    char file_name[256] = {0};
    size_t file_size = 0;
    FILE *file = NULL;

    snprintf(file_name, sizeof(file_name), state_filename_format, state_name);

    snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Loading '%s'.", file_name);
    debug_log(platform_top_debug_buff);

    file = fopen(file_name, "rb");

    if (file == NULL)
    {
        int err = errno;
        snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Failed to load '%s': %s", file_name, strerror(err));
        debug_log(platform_top_debug_buff);
        return;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);

    if (file_size != app_memory.serializable->size_bytes)
    {
        memory_release(&app_memory.serializable);
        app_memory.serializable = memory_allocate(file_size);
    }

    rewind(file);
    
    fread(app_memory.serializable->buffer, 1, file_size, file);
    fclose(file);
}

bool get_should_terminate(void)
{
    return should_terminate;
}

void set_should_terminate(bool value)
{
    should_terminate = value;
}

static void load_app(void)
{
    debug_log("Loading app layer.\n");

    if (lib_handle != NULL)
    {
        debug_log("Closing previously loaded handle.\n");
        dlclose(lib_handle);
        lib_handle = NULL;
        func_handle = NULL;
    }

    lib_handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);

    if (lib_handle == NULL )
    {
        snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Failed to load [%s]: %s\n", lib_path, dlerror());
        debug_log(platform_top_debug_buff);
        should_terminate = true;
        return;
    }

    app_setup = (AppSetupFunc)dlsym(lib_handle, app_setup_name);
    app_init = (AppInitFunc)dlsym(lib_handle, app_init_name);
    app_loop = (AppLoopFunc)dlsym(lib_handle, app_loop_name);
    app_exit = (AppExitFunc)dlsym(lib_handle, app_exit_name);

    struct stat lib_stat = {0};
    stat(lib_path, &lib_stat);
    lib_load_time = lib_stat.st_mtime;
}

int main(int argc, char **argv)
{
#define TERMINATION_POINT if (should_terminate) goto platform_termination

    static struct stat file_stat = {0};

    initialize_signal_handler();

    debug_init();
    debug_log("Program started.\n");

    if (argc > 1)
    {
        snprintf(lib_path, sizeof(lib_path), "%s", argv[1]);
        snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Using given library path: %s.\n", lib_path);
        debug_log(platform_top_debug_buff);
    }
    else
    {
        snprintf(platform_top_debug_buff, sizeof(platform_top_debug_buff), "Using default library path: %s.\n", lib_path);
        debug_log(platform_top_debug_buff);
    }

    load_app();
    TERMINATION_POINT;

    /// calling the app layer's setup function to establish platform settings and requirements.
    app_setup(&platform);
    TERMINATION_POINT;

    app_memory.serializable = memory_allocate(platform_settings.app_memory_serializable_bytes);
    TERMINATION_POINT;
    app_memory.ephemeral = memory_allocate(platform_settings.app_memory_ephemeral_bytes);
    TERMINATION_POINT;

    /// initializing platform modules according to given settings
    audio_init(&platform_settings, &app_memory.audio_buffer);
    gfx_init(&platform_settings, &app_memory.gfx_buffer);
    input_init(&platform_settings, &app_memory.input_buffer);

    if (app_init != NULL)
    {
        app_init(&platform, &app_memory);
    }
    else
    {
    }

    TERMINATION_POINT;

    while(!should_terminate)
    {
        time_mark_cycle_start();

        input_push_to_buffer(&platform_settings, app_memory.input_buffer);

        if (app_loop != NULL)
        {
            app_loop();
        }
        else
        {
        }

        if (gfx_get_debug_mode() == GFX_DEBUG_AUDIO)
        {
            gfx_audio_vis(app_memory.audio_buffer, audio_get_volume());
        }

        gfx_sync_buffer(app_memory.gfx_buffer);

        stat(lib_path, &file_stat);
        lib_modified_time = file_stat.st_mtime;

        if (lib_modified_time != lib_load_time)
        {
            debug_log("App modification detected.\n");
            load_app();
        }

        time_mark_cycle_end(platform_settings.gfx_frame_time_target_us);
        int64_t last_cycle_leftover_us = time_get_leftover_us();

        if (last_cycle_leftover_us > 0)
        {
            usleep(last_cycle_leftover_us);
            time_mark_cycle_end(platform_settings.gfx_frame_time_target_us);
        }
    }

platform_termination:

    if (app_exit != NULL) app_exit();

    if (lib_handle != NULL)
    {
        debug_log("Closing previously loaded handle.\n");
        dlclose(lib_handle);
        lib_handle = NULL;
        func_handle = NULL;
    }

    audio_deinit();
    gfx_deinit();

    memory_release(&app_memory.serializable);
    memory_release(&app_memory.ephemeral);

    free(app_memory.gfx_buffer);
    free(app_memory.audio_buffer);

    debug_log("Program halted, press ENTER to quit.");
    debug_break();

    return EXIT_SUCCESS;
}
