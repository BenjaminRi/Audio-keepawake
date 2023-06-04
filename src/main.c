#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_mixer.h>

#ifdef __unix__
#include <unistd.h>
#endif

// Log categories
// https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_log.h

unsigned int GetChunkTimeMs(Mix_Chunk* chunk) {
	// Taken with minor modifications from:
	// https://discourse.libsdl.org/t/time-length-of-sdl-mixer-chunks/12852
	int freq = 0;
	Uint16 fmt = 0;
	int chans = 0;
	// Chunks are converted to audio device format…
	if (!Mix_QuerySpec(&freq, &fmt, &chans)) {
		SDL_Log("Could not query spec: %s", SDL_GetError());
		return 0;
	}

	// Bytes / samplesize == sample points
	Uint32 points = (chunk->alen / ((fmt & 0xFF) / 8));

	// Sample points / channels == sample frames
	Uint32 frames = (points / chans);

	// (sample frames * 1000) / frequency == play length in ms
	return (unsigned int) ((frames * 1000) / freq);
}

typedef struct {
	char dev_name[64];
	bool audio_initialized;
	int buf_samples;
	int allowed_changes;
	char wave_filename[256];
	Mix_Chunk* wave_sound;
	unsigned int initial_loop_duration_ms;
	unsigned int callback_delay_ms;
	bool first_sound;
} AudioState;

AudioState* CreateAudio() {
	AudioState* audio = malloc(sizeof(AudioState));
	if(audio) {
		audio->dev_name[0] = '\0';
		audio->audio_initialized = false,
		audio->buf_samples = 2048,
		audio->allowed_changes = 0,
		audio->wave_filename[0] = '\0';
		audio->wave_sound = NULL;
		audio->initial_loop_duration_ms = 6000;
		audio->callback_delay_ms = 1200000; //10000; // 1'200'000 = 20 minutes
		audio->first_sound = true;
	}
	return audio;
}

bool AudioDevMatch(AudioState* audio, const char* other_dev_name) {
	// Match prefix instead of `sizeof(audio->dev_name)` to catch the
	// ` (2)` etc. duplicate devices after unplugging and replugging
	const size_t dev_name_len = strlen(audio->dev_name);
	return (strncmp(other_dev_name, audio->dev_name, dev_name_len) == 0);
}

void LoadWave(AudioState* audio) {
	if (audio->wave_sound != NULL) {
		SDL_Log("Chunk already loaded, free first");
		Mix_FreeChunk(audio->wave_sound);
		audio->wave_sound = NULL;
	}
	audio->wave_sound = Mix_LoadWAV(audio->wave_filename);
	if (audio->wave_sound == NULL) {
		SDL_Log("Couldn't load \"%s\": %s", audio->wave_filename, SDL_GetError());
	} else {
		SDL_Log("Mix_LoadWAV successful");
	}
}

void FreeWave(AudioState* audio) {
	if (audio->wave_sound != NULL) {
		SDL_Log("Free audio chunk");
		Mix_FreeChunk(audio->wave_sound);
		audio->wave_sound = NULL;
	}
}

void CloseAudio(AudioState* audio) {
	if(audio->audio_initialized) {
		SDL_Log("Close audio");
		FreeWave(audio);
		Mix_CloseAudio();
		audio->audio_initialized = false;
	}	
}

void OpenAudio(AudioState* audio) {
	if(audio->audio_initialized) {
		SDL_Log("Audio already open, close first");
		FreeWave(audio);
		Mix_CloseAudio();
		audio->audio_initialized = false;
	}
	
	const char* dev_to_use = NULL;
	const int playback_dev_count = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < playback_dev_count; ++i) {
		const char* current_dev = SDL_GetAudioDeviceName(i, 0);
		if(AudioDevMatch(audio, current_dev)) {
			// Take the last one, so don't break
			dev_to_use = current_dev;
		}
	}
	
	if(!dev_to_use) {
		SDL_Log("Could not find device \"%s\"", audio->dev_name);
		return;
	}
	
	if (Mix_OpenAudioDevice(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, audio->buf_samples, dev_to_use, audio->allowed_changes) == -1) {
		SDL_Log("Mix_OpenAudio: %s", Mix_GetError());
		audio->audio_initialized = false;
	} else {
		SDL_Log("Mix_OpenAudio successfully opened \"%s\"", dev_to_use);
		audio->audio_initialized = true;
		audio->first_sound = true;
	}
}

void PlayAudio(AudioState* audio) {
	if(audio->wave_sound) {
		if(audio->first_sound) {
			audio->first_sound = false;
			
			const unsigned int chunk_time_ms = GetChunkTimeMs(audio->wave_sound);
			const unsigned int initial_loop_duration_ms = audio->initial_loop_duration_ms ? (audio->initial_loop_duration_ms - 1) : 0;
			// `loops == 2` means it plays 3 times!
			// We play for the configured duration, rounded up until the wave audio finished playing
			// Therefore, the sounds may play a bit longer than specified
			const int loops = chunk_time_ms ? (initial_loop_duration_ms / chunk_time_ms) : 3;
			
			if(Mix_PlayChannel(0, audio->wave_sound, loops) == -1) {
				SDL_Log("Could not play audio: %s", Mix_GetError());
			} else {
				SDL_Log("Playing initial sound %i times, length %ums, config duration: %ums", loops + 1, chunk_time_ms, audio->initial_loop_duration_ms);
			}
		} else {
			const int loops = 0;
			if(Mix_PlayChannel(1, audio->wave_sound, loops) == -1) {
				SDL_Log("Could not play audio: %s", Mix_GetError());
			} else {
				SDL_Log("Playing sound once");
			}
		}
	} else {
		SDL_Log("Cannot play audio - wave sound not existing");
	}
}


void FreeAudio(AudioState* audio) {
	if(audio) {
		CloseAudio(audio);
		free(audio);
	}
}

void SetupAudio(AudioState* audio) {
	OpenAudio(audio);
	LoadWave(audio);
}

void RemoveTimer(SDL_TimerID* timer_id) {
	if(*timer_id != 0) {
		if(!SDL_RemoveTimer(*timer_id)) {
			SDL_Log("Timer removal of timer %i failed", (int) *timer_id);
		}
		*timer_id = 0;
	}
}

void LogAllOutputDevices() {
	const int playback_dev_count = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < playback_dev_count; ++i) {
		const char* current_dev = SDL_GetAudioDeviceName(i, 0);
		SDL_Log("Audio output device %i: \"%s\"", i, current_dev);
	}	
}

void LogAllDecoders() {
	const int music_decoder_count = Mix_GetNumMusicDecoders();
	SDL_Log("Number of audio decoders: %i", music_decoder_count);
	for (int i = 0; i < music_decoder_count; i++) {
		SDL_Log("Audio music decoder: %s", Mix_GetMusicDecoder(i));
	}
}


Uint32 timer_callback(Uint32 interval, void *param)
{
	// https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_timer.h
	// Timers can be removed up until this callback body returns 0
	// If this callback body returns 0, the timer removes itself
	
	// https://stackoverflow.com/questions/27414548/sdl-timers-and-waitevent
	// We work with events to do all the work on the main thread and to effectively become
	// race-condition free (remember, this callback runs in a separate thread in parallel)
	
	// Details about events can be found here:
	// https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_events.h
	
    SDL_Event event;
    SDL_UserEvent userevent;
    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return interval;
}

int main(int argc, char* argv) {
	
	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
		SDL_Log("Could not initialize SDL2");
		return 1;
	}
	
	{
		char* base_path = SDL_GetBasePath();
		SDL_Log("Binary base path: %s", base_path);
		SDL_free(base_path);
	}
	
	{
#ifdef __unix__
		char cwd_buf[4096];
		char* cwd = getcwd(cwd_buf, sizeof(cwd_buf));
		if(cwd) {
			SDL_Log("Current working directory: %s", cwd);
		} else {
			SDL_Log("Current working directory: (unknown)");
		}
#else
	SDL_Log("Current working directory: (not implemented)");
#endif
	}
	
	AudioState* audio = CreateAudio();
	snprintf(audio->dev_name, sizeof(audio->dev_name), "Steinberg UR22mkII  Analog Stereo");
	//snprintf(audio->dev_name, sizeof(audio->dev_name), "Sennheiser D 10 Mono");
	//snprintf(audio->dev_name, sizeof(audio->dev_name), "Logitech Stereo H650e Analog Stereo");
	//snprintf(audio->wave_filename, sizeof(audio->wave_filename), "type.wav");
	snprintf(audio->wave_filename, sizeof(audio->wave_filename), "10Hz.wav");
	
	LogAllOutputDevices();
	
	/*
	//int err = Mix_Init(MIX_INIT_OGG);
	//SDL_Log("Mix_Init(MIX_INIT_OGG) == %i", err);

	total = Mix_GetNumMusicDecoders();
	SDL_Log("Total decoders: %i", total);
	
	for (int i = 0; i < total; i++) {
		SDL_Log("Audio music decoder: %s", Mix_GetMusicDecoder(i));
	}*/
	
	//while(Mix_Playing(0))
	SDL_TimerID timer_id = 0;
	int running = true;
	while (running) {
		// Note: SDL_WaitEvent is buggy and gets stuck (blocking forever,
		// even if there are new events) with very high CPU load.
		// SDL_WaitEvent also polls aggressively in 1ms interval
		// See: https://github.com/libsdl-org/SDL/blob/SDL2/src/events/SDL_events.c#L1166
		// Hence, we can do better with a simple, naive implementation.
		SDL_Event e;
		while(SDL_PollEvent(&e) && running)
		{
			//SDL_Log("* SDL_PollEvent looping");
			switch(e.type)
			{
				case SDL_QUIT:
					SDL_Log("Program quit after %i ticks", e.quit.timestamp);
					running = false;
					break;
				case SDL_AUDIODEVICEADDED:
					{
						SDL_AudioDeviceEvent* ee = (SDL_AudioDeviceEvent*) &e;
						const char* evt_dev_name = SDL_GetAudioDeviceName(ee->which, ee->iscapture);
						SDL_Log("Audio device added: ID: %"PRIu32", Cap: %"PRIu8", Name: \"%s\"", ee->which, ee->iscapture, evt_dev_name);
						
						if(ee->iscapture == 0 && AudioDevMatch(audio, evt_dev_name)) {
							SDL_Log("Set timer for matching audio output device: \"%s\"", audio->dev_name);
							RemoveTimer(&timer_id);
							SetupAudio(audio);
							PlayAudio(audio);
							timer_id = SDL_AddTimer(audio->callback_delay_ms, timer_callback, NULL );
						}
					}
					break;
				case SDL_AUDIODEVICEREMOVED:
					{
						SDL_AudioDeviceEvent* ee = (SDL_AudioDeviceEvent*) &e;
						// Trigger a complete redetect of available hardware
						// This _sometimes_ helps preventing device `Foo` reappearing
						// as `Foo (2)` when it is plugged back in.
						SDL_GetNumAudioDevices(0);
						SDL_GetNumAudioDevices(1);
						
						// This only fires if an active playback is interrupted.
						// The "which" variable is not a device index, but a SDL_AudioDeviceID.
						SDL_AudioStatus status = SDL_GetAudioDeviceStatus(ee->which);
						switch (status) {
							case SDL_AUDIO_STOPPED:
								SDL_Log("Audio stopped - device unplugged?");
								SDL_Log("Remove timer for audio output device: \"%s\"", audio->dev_name);
								RemoveTimer(&timer_id);
								break;
							case SDL_AUDIO_PLAYING:
								SDL_Log("Audio playing");
								break;
							case SDL_AUDIO_PAUSED:
								SDL_Log("Audio paused");
								break;
							default:
								SDL_Log("Audio in unknown state!");
								break;
						}
					}
					break;
				case SDL_USEREVENT:
					{
						SDL_Log("Sound timer triggered");

						PlayAudio(audio);
					}
					break;
				default:
					// https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_events.h
					SDL_Log("Got unknown event 0x%x", e.type);
					break;
			}
		}
		//SDL_Log("* SDL_PollEvent done");
		SDL_Delay(1000); // 1 second intervals are more than enough for our purposes
	}
	
	RemoveTimer(&timer_id);
	
	FreeAudio(audio);
	
	SDL_Quit();
	
	return 0;
}
