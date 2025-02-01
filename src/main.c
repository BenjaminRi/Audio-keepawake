#include <SDL3/SDL.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Note: SDL uses timer ID 0 to indicate failure, so this consistent with SDL's API
#define INVALID_TIMER_ID 0

const char* recording_str(bool recording) { return recording ? "recording" : "playback"; }

float float_min(float a, float b) { return a < b ? a : b; }

void RemoveTimer(SDL_TimerID* timer_id) {
    if (*timer_id != INVALID_TIMER_ID) {
        if (!SDL_RemoveTimer(*timer_id)) {
            SDL_Log("Timer removal of timer %" PRIu32 " failed", *timer_id);
        }
        *timer_id = INVALID_TIMER_ID;
    }
}

uint32_t timer_callback(void* userdata, SDL_TimerID timerID, uint32_t interval) {
    // Whenever we get a timer callback, we queue an event and do nothing else.
    // The event handler will do all the work on the main thread, so we become
    // race-condition free (remember, this callback runs in a separate thread
    // in parallel)
    // https://stackoverflow.com/questions/27414548/sdl-timers-and-waitevent

    SDL_Event event;
    SDL_UserEvent userevent;
    userevent.type = SDL_EVENT_USER;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_EVENT_USER;
    event.user = userevent;

    SDL_PushEvent(&event);
    return interval;
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    char dev_name[64];
    snprintf(dev_name, sizeof(dev_name), "Steinberg UR22mkII  Analog Stereo");
    snprintf(dev_name, sizeof(dev_name), "Logitech Stereo H650e Analog Stereo");

    uint32_t callback_delay_ms = 4000;

    SDL_Log("SDL Audio subsystem initialized. Waiting for audio events...");

    SDL_AudioSpec spec;
    spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 8000;

    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int current_sine_sample = 0;
    float sound_duration_sec = 2.0;
    float fade_duration_sec = 0.15; // both fade-in and fade-out will have this duration

    const int frequency = 440;

    const int sound_sample_count = (int)(sound_duration_sec * 8000);
    const int fade_sample_count = (int)(fade_duration_sec * 8000);

    float* const samples = malloc(sound_sample_count * sizeof(float));

    SDL_TimerID timer_id = INVALID_TIMER_ID;
    RemoveTimer(&timer_id);
    timer_id = SDL_AddTimer(callback_delay_ms, timer_callback, NULL);

    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
                break;
            }

            switch (event.type) {
                case SDL_EVENT_AUDIO_DEVICE_ADDED: {
                    const char* name = SDL_GetAudioDeviceName(event.adevice.which);
                    name = name ? name : "[Unknown]";
                    const bool recording = event.adevice.recording;
                    SDL_Log("Audio device added: ID: %d, device type: %s, name: %s", event.adevice.which,
                            recording_str(recording), name);

                    if (!recording && strncmp(name, dev_name, sizeof(dev_name)) == 0) {
                        SDL_AudioDeviceID devid = SDL_OpenAudioDevice(event.adevice.which, &spec);
                        if (devid == 0) {
                            SDL_Log("Couldn't open device: %s", SDL_GetError());
                            SDL_Quit();
                            return 1;
                        }
                        SDL_UnbindAudioStream(stream);
                        if (!SDL_BindAudioStream(devid, stream)) {
                            SDL_Log("Couldn't bind audio stream: %s", SDL_GetError());
                            SDL_Quit();
                            return 1;
                        }
                        SDL_Log("Successfully opened and bound device, new ID: %d", devid);
                    }
                    break;
                }
                case SDL_EVENT_AUDIO_DEVICE_REMOVED: {
                    const char* name = SDL_GetAudioDeviceName(event.adevice.which);
                    name = name ? name : "[Unknown]";
                    const bool recording = event.adevice.recording;
                    SDL_Log("Audio device removed: ID: %d, device type: %s, name: %s", event.adevice.which,
                            recording_str(recording), name);
                    break;
                }
                case SDL_EVENT_USER: {
                    SDL_Log("Sound timer triggered");

                    const int minimum_audio = (8000 * sizeof(float)) / 2; // 8000 float samples per second. Half of that.
                    if (SDL_GetAudioStreamAvailable(stream) < minimum_audio) {
                        // generate a 440Hz pure tone
                        for (int i = 0; i < sound_sample_count; i++) {
                            const float phase = current_sine_sample * frequency / 8000.0f;

                            float amplitude = 1.0;
                            if (i <= fade_sample_count) {
                                // Linear fade-in
                                amplitude = float_min(amplitude, (float)i / (float)fade_sample_count);
                            }
                            if (i >= sound_sample_count - fade_sample_count) {
                                // Linear fade-out
                                amplitude = float_min(amplitude, (float)(sound_sample_count - i) / (float)fade_sample_count);
                            }

                            samples[i] = amplitude * SDL_sinf(phase * 2 * SDL_PI_F);
                            current_sine_sample++;
                        }

                        // wrapping around to avoid floating-point errors
                        current_sine_sample %= 8000;

                        // feed the new data to the stream. It will queue at the end, and
                        // trickle out as the hardware needs more data.
                        SDL_PutAudioStreamData(stream, samples, sound_sample_count * sizeof(float));
                    }
                    break;
                }
            }
        }
    }

    free(samples);
    SDL_Quit();
    return 0;
}
