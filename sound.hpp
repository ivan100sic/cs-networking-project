#ifndef CSNPS_SOUND_HPP
#define CSNPS_SOUND_HPP

#include "common.hpp"
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <iostream>
#include <alsa/asoundlib.h>
using namespace std;
using namespace std::chrono;

const size_t BUFFER_SAMPLES = 2048;

struct SoundRecorder {

	snd_pcm_t* handle;
	snd_pcm_hw_params_t* params;

	SoundRecorder() {
		// default uredjaj je "hw:0,0"
		snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0); // blocking pozivi
		snd_pcm_hw_params_malloc(&params);
		snd_pcm_hw_params_any(handle, params);
		snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
		
		unsigned int sample_rate = 44100;
		snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, nullptr);

		snd_pcm_hw_params_set_channels(handle, params, 2);
		snd_pcm_hw_params(handle, params);
		snd_pcm_hw_params_free(params);
	}

	// Zovi callback, predaj ceo buffer kao vector<uint32_t>
	template<class T>
	void run (T callback) {
		vector<uint32_t> buf;
		while (1) {
			buf.resize(BUFFER_SAMPLES);
			snd_pcm_readi(handle, buf.data(), BUFFER_SAMPLES);
			callback(move(buf));
		}
	}

	~SoundRecorder() {
		snd_pcm_close(handle);
	}
};

struct SoundPlayer {

	snd_pcm_t* handle;
	snd_pcm_hw_params_t* params;

	queue<vector<uint32_t>> q;
	mutex mtx;

	SoundPlayer() {
		int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0); // blocking pozivi
		// realno jedino mesto gde moze da se desi nesto
		if (err < 0) {
			cerr << "Error opening playback stream\n";
			handle = nullptr;
			params = nullptr;
			return;
		}

		snd_pcm_hw_params_malloc(&params);
		snd_pcm_hw_params_any(handle, params);
		snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
		snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
		unsigned int sample_rate = 44100;
		snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, nullptr);
		snd_pcm_hw_params_set_channels(handle, params, 2);
		snd_pcm_hw_params_set_buffer_size(handle, params, BUFFER_SAMPLES);

		snd_pcm_hw_params(handle, params);
		snd_pcm_hw_params_free(params);
	}

	static void snd_write_wrapper(snd_pcm_t* handle,
		const vector<uint32_t>& buff, int& health)
	{
		int err = snd_pcm_writei(handle, buff.data(), buff.size());
		if (err < 0) {
			cerr << "Error code: " << err << '\n';
			health = 0;
		}
	}

	// run kreira novi thread i izlazi
	// kreirani thread nastavlja da pusta zvuke
	static void run_impl(SoundPlayer* sp) {
		const int QUEUE_SIZE_OPTIMAL = 4;
		const int QUEUE_SIZE_CRITICAL = 2;
		const int QUEUE_SIZE_TOO_BIG = 256;
		const int MAX_HEALTH = 1024;

		int health = MAX_HEALTH;
		int filled = 0;
		while (health > 0) {
			{
				unique_lock<mutex> lock(sp->mtx);
				size_t qsz = sp->q.size();
				cerr << con_erase(40) << "H: " << health << " Q: ";

				if (qsz < QUEUE_SIZE_OPTIMAL && !filled) {
					// cekamo da se napuni jos malo
					lock.unlock();
					this_thread::sleep_for(milliseconds(60));
				} else if (qsz >= QUEUE_SIZE_OPTIMAL && !filled) {
					lock.unlock();
					filled = 1;
				}
			}

			unique_lock<mutex> lock(sp->mtx);
			if (filled) {
				size_t qsz = sp->q.size();
				if (qsz <= QUEUE_SIZE_CRITICAL) {
					lock.unlock();
					health--;
					vector<uint32_t> buff(BUFFER_SAMPLES, 0);
					snd_write_wrapper(sp->handle, buff, health);
				} else if (qsz > QUEUE_SIZE_TOO_BIG) {
					sp->q.pop();
				} else {
					health = MAX_HEALTH;
					vector<uint32_t> a = move(sp->q.front()); sp->q.pop();
					lock.unlock();
					snd_write_wrapper(sp->handle, a, health);
				}
			}
		}
		cerr << "Out of health, stopping...\n";
	}

	void run() {
		thread t(run_impl, this);
		t.detach();
	}

	void add(vector<uint32_t>&& a) {
		unique_lock<mutex> lock(mtx);
		q.push(a);
	}

	~SoundPlayer () {
		if (handle) {
			snd_pcm_close(handle);
		}
	}
};

#endif