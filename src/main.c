#include <SDL3/SDL.h>
#include <stdio.h>

const char* recording_str(bool recording) {
	return recording ? "recording" : "playback";
}

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		SDL_Quit();
        return 1;
    }

    SDL_Log("SDL Audio subsystem initialized. Waiting for audio events...\n");

	SDL_AudioSpec spec;
	spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 8000;
    //SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
		SDL_Quit();
        return 1;
    }


	SDL_AudioDeviceID devid = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
	if(devid == 0) {
		SDL_Log("Couldn't open device: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	if(!SDL_BindAudioStream(devid, stream)) {
		SDL_Log("Couldn't bind audio stream: %s", SDL_GetError());
		SDL_Quit();
        return 1;
	}

	//SDL_ResumeAudioStreamDevice(stream);


	int current_sine_sample = 0;
	const int minimum_audio = (8000 * sizeof (float)) / 2; // 8000 float samples per second. Half of that.
    if (SDL_GetAudioStreamAvailable(stream) < minimum_audio) {
        static float samples[512]; // this will feed 512 samples each frame until we get to our maximum.
        int i;

        // generate a 440Hz pure tone
        for (i = 0; i < SDL_arraysize(samples); i++) {
            const int freq = 440;
            const float phase = current_sine_sample * freq / 8000.0f;
            samples[i] = SDL_sinf(phase * 2 * SDL_PI_F);
            current_sine_sample++;
        }

        // wrapping around to avoid floating-point errors
        current_sine_sample %= 8000;

        // feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data.
        SDL_PutAudioStreamData(stream, samples, sizeof (samples));
    }

    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
                break;
            } else if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED) {
				const char* name = SDL_GetAudioDeviceName(event.adevice.which);
				name = name ? name : "[Unknown]";
                SDL_Log("Audio device added: ID = %d, device type: %s, name: %s\n",
                       event.adevice.which, recording_str(event.adevice.recording), name);
            } else if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {
				const char* name = SDL_GetAudioDeviceName(event.adevice.which);
				name = name ? name : "[Unknown]";
                SDL_Log("Audio device removed: ID = %d, device type: %s, name: %s\n",
                       event.adevice.which, recording_str(event.adevice.recording), name);
            }
        }
    }

    SDL_Quit();
    return 0;
}
