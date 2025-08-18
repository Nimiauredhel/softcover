#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>                                                                                                                                                                                                                        
#include <time.h>                                                                                                                                                                                                                         
#include <sys/stat.h>                                                                                                                                                                                                                     

#include "terminal_ncurses.h"
#include "terminal_portaudio.h"

#include "softcover_platform.h"

#ifndef LIB_NAME
#define LIB_NAME ""
#endif

Memory_t* memory_allocate(size_t size);
void memory_release(Memory_t *memory);
void storage_save_state(char *state_name);
void storage_load_state(char *state_name);

static const Platform_t platform =
{
    .memory_allocate = memory_allocate,
    .memory_release = memory_release,
    .gfx_clear_buffer = gfx_clear_buffer,
    .gfx_draw_texture = gfx_draw_texture,
    .gfx_load_texture = gfx_load_texture,
    .input_read = input_read,
    .audio_play_chunk = audio_play_chunk,
    .storage_save_state = storage_save_state,
    .storage_load_state = storage_load_state,
};

static const char *app_init_name = "app_init";
static const char *app_loop_name = "app_loop";

static const char *state_filename_format = "%.state";

static char lib_path[256] = LIB_NAME;

static time_t lib_load_time = {0};
static time_t lib_modified_time = {0};

static void *lib_handle = NULL;
static void (*func_handle)(void) = NULL;

static AppInitFunc app_init;
static AppLoopFunc app_loop;

static Memory_t *app_main_memory = NULL;

Memory_t* memory_allocate(size_t size)
{
    Memory_t *memory = (Memory_t *)malloc(size + sizeof(Memory_t));
    explicit_bzero(memory->buffer, size);
    memory->size = size;
    return memory;
}

void memory_release(Memory_t *memory)
{
    free(memory);
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
    }
    
    fwrite(app_main_memory->buffer, 1, app_main_memory->size, file);
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
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);

    if (file_size != app_main_memory->size)
    {
        memory_release(app_main_memory);
        app_main_memory = memory_allocate(file_size);
    }

    rewind(file);
    
    fread(app_main_memory->buffer, 1, file_size, file);
    fclose(file);
}

static void load_app(void)
{
    //printf("Loading dynamic library.\n");

    if (lib_handle != NULL)
    {
        //printf("Closing previously loaded handle.\n");
        dlclose(lib_handle);
        lib_handle = NULL;
        func_handle = NULL;
    }

    lib_handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);

    if (lib_handle == NULL )
    {
        //printf("Failed to load [%s]: %s\n", lib_path, dlerror());
        exit(EXIT_FAILURE);
    }

    app_init = dlsym(lib_handle, app_init_name);
    app_loop = dlsym(lib_handle, app_loop_name);

    struct stat lib_stat = {0};
    stat(lib_path, &lib_stat);
    lib_load_time = lib_stat.st_mtime;
}

int main(int argc, char **argv)
{
    static struct stat file_stat = {0};

    printf("Program started.\n");

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
    audio_init();
    gfx_init();

    if (app_init != NULL)
    {
        app_init(&platform, &app_main_memory);
    }
    else
    {
    }

    for(;;)
    {
        if (app_loop != NULL)
        {
            app_loop(&platform, app_main_memory);
        }
        else
        {
        }

        gfx_sync_buffer();

        stat(lib_path, &file_stat);
        lib_modified_time = file_stat.st_mtime;

        if (lib_modified_time != lib_load_time)
        {
            //printf("Library modification detected.\n");
            load_app();
        }

        usleep(16000);
    }

    return EXIT_SUCCESS;
}
