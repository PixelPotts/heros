#include "audio.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

// ── Init / Cleanup ──────────────────────────────────────────────

bool AudioManager::init() {
    if (initialized_) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "AudioManager: SDL_InitSubSystem(AUDIO): %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want{}, have{};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = this;

    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_id_ == 0) {
        fprintf(stderr, "AudioManager: SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(device_id_, 0); // start playing
    memset(tones_, 0, sizeof(tones_));
    tone_count_ = 0;
    initialized_ = true;
    fprintf(stderr, "AudioManager: initialized (device %u, %d Hz)\n", device_id_, have.freq);
    return true;
}

void AudioManager::cleanup() {
    if (device_id_) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    initialized_ = false;
}

// ── Volume ──────────────────────────────────────────────────────

void AudioManager::set_volume(float v) {
    volume_ = std::max(0.0f, std::min(1.0f, v));
}

// ── Play system sounds ──────────────────────────────────────────

void AudioManager::play(SystemSound sound) {
    if (!initialized_ || muted_) return;

    switch (sound) {
    case SystemSound::Notify:
        play_tone(880, 80);
        play_tone(1100, 60);
        break;
    case SystemSound::Alert:
        play_tone(600, 120);
        play_tone(400, 120);
        break;
    case SystemSound::Click:
        play_tone(1200, 20);
        break;
    case SystemSound::Error:
        play_tone(300, 150);
        play_tone(200, 200);
        break;
    case SystemSound::Startup:
        play_tone(440, 100);
        play_tone(660, 100);
        play_tone(880, 150);
        break;
    case SystemSound::Lock:
        play_tone(660, 60);
        play_tone(440, 80);
        break;
    case SystemSound::Unlock:
        play_tone(440, 60);
        play_tone(660, 80);
        break;
    }
}

void AudioManager::play_tone(float freq_hz, int duration_ms) {
    if (!initialized_ || muted_) return;
    queue_tone(freq_hz, duration_ms);
}

// ── Tone queue (lock-free since SDL callback is single-threaded) ─

void AudioManager::queue_tone(float freq, int duration_ms) {
    SDL_LockAudioDevice(device_id_);
    if (tone_count_ < MAX_TONES) {
        tones_[tone_count_].freq = freq;
        tones_[tone_count_].samples_remaining = SAMPLE_RATE * duration_ms / 1000;
        tones_[tone_count_].phase = 0;
        tone_count_++;
    }
    SDL_UnlockAudioDevice(device_id_);
}

// ── Audio callback ──────────────────────────────────────────────

void AudioManager::audio_callback(void* userdata, uint8_t* stream, int len) {
    auto* self = static_cast<AudioManager*>(userdata);
    int sample_count = len / sizeof(int16_t);
    self->fill_audio(reinterpret_cast<int16_t*>(stream), sample_count);
}

void AudioManager::fill_audio(int16_t* buffer, int sample_count) {
    memset(buffer, 0, sample_count * sizeof(int16_t));

    if (muted_ || tone_count_ == 0) return;

    for (int i = 0; i < sample_count; i++) {
        float mixed = 0;
        for (int t = 0; t < tone_count_; t++) {
            if (tones_[t].samples_remaining <= 0) continue;

            float phase = (float)tones_[t].phase / SAMPLE_RATE;
            float val = sinf(2.0f * (float)M_PI * tones_[t].freq * phase);

            // Simple envelope: fade out over last 20% of duration
            float env = 1.0f;
            int total = tones_[t].phase + tones_[t].samples_remaining;
            float progress = (float)tones_[t].phase / total;
            if (progress > 0.8f)
                env = (1.0f - progress) / 0.2f;

            mixed += val * env;
            tones_[t].phase++;
            tones_[t].samples_remaining--;
        }

        mixed *= volume_ * 0.3f; // scale down to avoid clipping
        int32_t sample = (int32_t)(mixed * 32767);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buffer[i] = (int16_t)sample;
    }

    // Remove finished tones
    int write = 0;
    for (int t = 0; t < tone_count_; t++) {
        if (tones_[t].samples_remaining > 0) {
            if (write != t) tones_[write] = tones_[t];
            write++;
        }
    }
    tone_count_ = write;
}
