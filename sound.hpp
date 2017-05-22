#ifndef CSNPS_SOUND_HPP
#define CSNPS_SOUND_HPP

#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <iostream>
#include <alsa/asoundlib.h>
using namespace std;
using namespace std::chrono;

const snd_pcm_uframes_t BUFFER_SAMPLES = 1024;

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

	// Zovi callback, predaj ceo buffer kao 2-kanalni vector<short>
	// duzine 2 * buffer_size
	template<class T>
	void run (T callback) {
		short* buf = new short[2 * BUFFER_SAMPLES];
		while (1) {
			snd_pcm_readi(handle, buf, BUFFER_SAMPLES);
			vector<short> vec(buf, buf+2*BUFFER_SAMPLES);
			callback(move(vec));
		}
		// za slucaj da nekad odlucim da promenim while(1)
		delete[] buf;
	}

	~SoundRecorder() {
		snd_pcm_close(handle);
	}
};

struct SoundPlayer {

	snd_pcm_t* handle;
	snd_pcm_hw_params_t* params;

	queue<vector<int16_t>> q;
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

	// run kreira novi thread i izlazi
	// kreirani thread nastavlja da pusta zvuke
	static void run_impl(SoundPlayer* sp) {
		const int QUEUE_SIZE_OPTIMAL = 4;
		const int QUEUE_SIZE_CRITICAL = 2;

		int health = 1;
		int filled = 0;
		while (health > 0) {
			{
				unique_lock<mutex> lock(sp->mtx);
				if (sp->q.size() < QUEUE_SIZE_OPTIMAL && !filled) {
					// cekamo da se napuni jos malo
					lock.unlock();
					this_thread::sleep_for(milliseconds(60));
				} else if (sp->q.size() >= QUEUE_SIZE_OPTIMAL && !filled) {
					lock.unlock();
					filled = 1;
				}
			}

			unique_lock<mutex> lock(sp->mtx);
			if (filled) {
				size_t qsz = sp->q.size();
				vector<int16_t> a = sp->q.front(); sp->q.pop();
				lock.unlock();
				int16_t* buff = new int16_t[a.size()];
				for (size_t i=0; i<a.size(); i++) buff[i] = a[i];
				// error handling?
				int err = snd_pcm_writei(sp->handle, buff, BUFFER_SAMPLES);
				if (err < 0) {
					cerr << "err " << err << '\n';
					health = 0;
				}
				// ako si ispod granice, pusti ga jos jednom
				if (qsz <= QUEUE_SIZE_CRITICAL) {
					cerr << "Hic!\n";
					int err = snd_pcm_writei(sp->handle, buff, BUFFER_SAMPLES);
					if (err < 0) {
						cerr << "err " << err << '\n';
						health = 0;
					}
				}

				delete[] buff;
			}
		}
	}

	void run() {
		thread t(run_impl, this);
		t.detach();
	}

	void add(const vector<int16_t>& a) {
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