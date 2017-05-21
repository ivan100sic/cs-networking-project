#include <cstdio>
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <alsa/asoundlib.h>
using namespace std;
using namespace std::chrono;

int main () {
	const size_t max_samples = 16384;
	short buf[2 * max_samples];

	snd_pcm_t* handle;
	snd_pcm_hw_params_t* params;

	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0); // blocking pozivi

	cerr << "Here!\n";
	cerr << err << '\n';

	snd_pcm_hw_params_malloc(&params);
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	unsigned int sample_rate = 44100;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, nullptr);
	snd_pcm_hw_params_set_channels(handle, params, 2);
	unsigned int buffer_time = 23212;
	snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, nullptr);
	cerr << buffer_time << '\n';
	
	snd_pcm_uframes_t rrrrr;
	snd_pcm_hw_params_get_buffer_size(params, &rrrrr);
	cerr << "Buffer size is " << rrrrr << '\n';

	snd_pcm_hw_params(handle, params);
	snd_pcm_hw_params_free(params);

	double t = 0.0;
	double dt = 1. / 44100;

	for (int i = 0; i < 10; i++) {

		for (size_t j=0; j<max_samples; j++) {
			buf[2*j] = 1000 * sin(t * 440 * 2 * 3.141592);
			buf[2*j+1] = 1000 * sin(t * 461 * 2 * 3.141592);
			// buf[2*j] = 4153 ^ j * (j + 1);
			t += dt;
		}

		if ((err = snd_pcm_writei(handle, buf, max_samples)) != max_samples) {
			cerr << "Write to interface failed \n";
			cerr << err << '\n';
		}
	}

	snd_pcm_close(handle);	
}
