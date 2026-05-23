#pragma once
#include <string>
#include <cstdint>

// ── System Sound types ──────────────────────────────────────────

enum class SystemSound {
    Notify,
    Alert,
    Click,
    Error,
    Startup,
    Lock,
    Unlock
};

// ── Audio Manager ───────────────────────────────────────────────
// Lightweight audio subsystem built on SDL2 audio.
// Generates simple synth tones — no SDL2_mixer dependency needed.

class AudioManager {
public:
    bool init();
    void cleanup();

    // Master volume: 0.0 – 1.0
    float volume() const { return volume_; }
    void set_volume(float v);

    bool muted() const { return muted_; }
    void set_muted(bool m) { muted_ = m; }
    void toggle_mute() { muted_ = !muted_; }

    // Play a built-in system sound
    void play(SystemSound sound);

    // Play a tone: frequency Hz, duration ms
    void play_tone(float freq_hz, int duration_ms);

    // Status
    bool is_initialized() const { return initialized_; }

private:
    bool initialized_ = false;
    float volume_ = 0.7f;
    bool muted_ = false;

    // Synth state for the audio callback
    struct ToneRequest {
        float freq;
        int samples_remaining;
        int phase;
    };

    static const int SAMPLE_RATE = 44100;
    static const int MAX_TONES = 8;
    ToneRequest tones_[MAX_TONES] = {};
    int tone_count_ = 0;

    void queue_tone(float freq, int duration_ms);

    // SDL audio callback
    static void audio_callback(void* userdata, uint8_t* stream, int len);
    void fill_audio(int16_t* buffer, int sample_count);

    uint32_t device_id_ = 0;
};
