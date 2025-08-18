#include "terminal_portaudio.h"

#include <stdlib.h>

static PaStream *audio_stream;
static AudioBuffer_t audio_user_buffer = {0};

static int paStreamCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    /* Prevent unused variable warning. */
    (void) inputBuffer;

    /* Cast data passed through stream to our structure. */
    AudioBuffer_t *data = (AudioBuffer_t*)userData; 

    float *out = (float*)outputBuffer;
    
    for(uint32_t i = 0; i < framesPerBuffer; i++)
    {
        if (data->head == data->tail)
        {
            out[i] = 0.0f;
            continue;
        }

        out[i] = data->buffer[data->head];
        data->head++;
        if (data->head >= data->length) data->head = 0;
    }

    return 0;
}

static void set_audio(PaStream *stream, bool active)
{
    PaError err = paNoError;
    bool was_active = Pa_IsStreamActive(stream);

    if (active != was_active)
    {
        if (active) err = Pa_StartStream(stream);
        else err = Pa_StopStream(stream);
    }

    if(err != paNoError)
    {
        // TODO: handle error
    }
}

void audio_play_chunk(float *chunk, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        audio_user_buffer.buffer[audio_user_buffer.tail] = chunk[i];

        audio_user_buffer.tail += 1;

        if (audio_user_buffer.tail >= audio_user_buffer.length) audio_user_buffer.tail = 0;

        if (audio_user_buffer.tail == audio_user_buffer.head)
        {
            break;
        }
    }
}

void audio_init(void)
{
    PaError err;

    err = Pa_Initialize();

    if(err != paNoError)
    {
        // TODO: handle error
    }

    audio_user_buffer.head = 0;
    audio_user_buffer.tail = 0;
    audio_user_buffer.length = AUDIO_USER_BUFFER_LENGTH;

    PaStreamParameters output_parameters = {0};
    output_parameters.device = Pa_GetDefaultOutputDevice(); /* default input device */
    output_parameters.channelCount = 2;
    output_parameters.sampleFormat = paFloat32;
    output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&audio_stream,
                               NULL,
                               &output_parameters,
                               44100.0,
                               paFramesPerBufferUnspecified,        
                               paNoFlag,
                               paStreamCallback,
                               &audio_user_buffer);
    if(err != paNoError)
    {
        // TODO: handle error
    }

    set_audio(audio_stream, true);
}

void deinit_audio(PaStream *stream)
{
    PaError err;

    err = Pa_CloseStream(stream);
    err = Pa_Terminate();

    // TODO: handle PA errors ..
}
