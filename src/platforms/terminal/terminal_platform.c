#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include "terminal_utils.h"
#include "terminal_ncurses.h"
#include "terminal_portaudio.h"

#include "softcover_platform.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

Memory_t* memory_allocate(size_t size);
void memory_release(Memory_t **memory_pptr);
void storage_save_state(char *state_name);
void storage_load_state(char *state_name);
bool get_should_terminate(void);
void set_should_terminate(bool value);

static const char *state_filename_format = "%.state";

static char lib_path[256] = LIB_NAME;

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

static Memory_t *app_main_memory = NULL;

static const PlatformCapabilities_t capabilities =
{
    .app_memory_limit_bytes = 128*1024,
    .gfx_frame_time_min_us = 8000,
};

static const PlatformSettings_t default_settings =
{
    .app_memory_required_bytes = capabilities.app_memory_limit_bytes,
    .gfx_frame_time_target_us = capabilities.gfx_frame_time_min_us,
};

static PlatformSettings_t platform_settings = default_settings;

static const Platform_t platform =
{
    .capabilities = &capabilities,
    .settings = &platform_settings,

    .memory_allocate = memory_allocate,
    .memory_release = memory_release,

    .gfx_clear_buffer = gfx_clear_buffer,
    .gfx_draw_texture = gfx_draw_texture,
    .gfx_load_texture = gfx_load_texture,

    .input_read = input_read,

    .audio_play_chunk = audio_play_chunk,

    .storage_save_state = storage_save_state,
    .storage_load_state = storage_load_state,

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
        debug_log("Failed to allocate main memory.");
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

    file = fopen(file_name, "wb");

    if (file == NULL)
    {
        perror("Failed to create file");
        //exit(EXIT_FAILURE);
        return;
    }
    
    fwrite(app_main_memory->buffer, 1, app_main_memory->size_bytes, file);
    fclose(file);
}

void storage_load_state(char *state_name)
{
    char file_name[256] = {0};
    size_t file_size = 0;
    FILE *file = NULL;

    snprintf(file_name, sizeof(file_name), state_filename_format, state_name);

    file = fopen(file_name, "rb");

    if (file == NULL)
    {
        perror("Failed to open file");
        //exit(EXIT_FAILURE);
        return;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);

    if (file_size != app_main_memory->size_bytes)
    {
        memory_release(&app_main_memory);
        app_main_memory = memory_allocate(file_size);
    }

    rewind(file);
    
    fread(app_main_memory->buffer, 1, file_size, file);
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
        //printf("Failed to load [%s]: %s\n", lib_path, dlerror());
        debug_log("Failed to load app layer.");
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

    printf("Program started.\n");

    initialize_signal_handler();

    if (argc > 1)
    {
        snprintf(lib_path, sizeof(lib_path), "%s", argv[1]);
        printf("Using given library path: %s.\n", lib_path);
    }
    else
    {
        printf("Using default library path: %s.\n", lib_path);
    }

    load_app();
    TERMINATION_POINT;

    /// calling the app layer's setup function to establish platform settings and requirements.
    app_setup(&platform);
    TERMINATION_POINT;

    /// initializing platform modules according to given settings
    /// TODO: use given settings
    audio_init();
    gfx_init();

    app_main_memory = memory_allocate(platform_settings.app_memory_required_bytes);
    TERMINATION_POINT;

    if (app_init != NULL)
    {
        app_init(&platform, app_main_memory);
    }
    else
    {
    }

    while(!should_terminate)
    {
        if (app_loop != NULL)
        {
            app_loop(&platform, app_main_memory);
        }
        else
        {
        }

        gfx_sync_buffer();
        gfx_audio_vis(audio_get_buffer_readonly());

        stat(lib_path, &file_stat);
        lib_modified_time = file_stat.st_mtime;

        if (lib_modified_time != lib_load_time)
        {
            //printf("Library modification detected.\n");
            load_app();
        }

        usleep(16000);
    }

platform_termination:

    if (app_exit != NULL) app_exit(&platform, app_main_memory);
    memory_release(&app_main_memory);

    debug_log("Program halted, press ENTER to quit.");
    debug_break();

    audio_deinit();
    gfx_deinit();

    return EXIT_SUCCESS;
}
