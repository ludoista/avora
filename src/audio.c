#include "visualizer.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AUDIO_SAMPLE_RATE 44100
#define TWO_PI            6.28318530718f

static Sound g_sfx[SFX_COUNT];
static bool  g_audio_ready = false;

static Sound build_procedural_sound(const short *samples, int frame_count)
{
    Sound snd = { 0 };
    if (!g_audio_ready || !samples || frame_count <= 0) return snd;

    short *pcm = (short *)malloc((size_t)frame_count * sizeof(short));
    if (!pcm) return snd;
    memcpy(pcm, samples, (size_t)frame_count * sizeof(short));

    Wave w = { 0 };
    w.frameCount = (unsigned int)frame_count;
    w.sampleRate = AUDIO_SAMPLE_RATE;
    w.sampleSize = 16;
    w.channels   = 1;
    w.data       = pcm;

    snd = LoadSoundFromWave(w);
    UnloadWave(w); /* Frees pcm buffer after uploading to miniaudio */
    return snd;
}

void audio_engine_init(void)
{
    memset(g_sfx, 0, sizeof(g_sfx));
    g_audio_ready = false;

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        return; /* 100% Fail-safe on headless / no-audio Linux systems */
    }
    g_audio_ready = true;

    /* 1. SFX_STEP_TICK: Ultra-short mechanical tick (~18ms) */
    {
        int frames = (AUDIO_SAMPLE_RATE * 18) / 1000;
        short buf[1024];
        float phase = 0.0f;
        for (int i = 0; i < frames && i < 1024; i++) {
            float t   = (float)i / (float)AUDIO_SAMPLE_RATE;
            float env = expf(-t * 240.0f);
            float f   = 280.0f + 720.0f * expf(-t * 320.0f);
            phase    += TWO_PI * f / (float)AUDIO_SAMPLE_RATE;
            float s   = sinf(phase) * env * 0.45f;
            buf[i]    = (short)(s * 32767.0f);
        }
        g_sfx[SFX_STEP_TICK] = build_procedural_sound(buf, frames);
    }

    /* 2. SFX_NODE_POP: Soft organic pop (~48ms, pitch-shifted dynamically on playback) */
    {
        int frames = (AUDIO_SAMPLE_RATE * 48) / 1000;
        short buf[2500];
        float phase = 0.0f;
        for (int i = 0; i < frames && i < 2500; i++) {
            float t   = (float)i / (float)AUDIO_SAMPLE_RATE;
            float env = sinf((float)i / (float)frames * 3.14159265f) * expf(-t * 42.0f);
            float f   = 390.0f + 290.0f * ((float)i / (float)frames);
            phase    += TWO_PI * f / (float)AUDIO_SAMPLE_RATE;
            float s   = sinf(phase) * env * 0.52f;
            buf[i]    = (short)(s * 32767.0f);
        }
        g_sfx[SFX_NODE_POP] = build_procedural_sound(buf, frames);
    }

    /* 3. SFX_COMPILE_CHIME: Clean 2-note ascending chime (~180ms, C5 -> G5) */
    {
        int frames = (AUDIO_SAMPLE_RATE * 180) / 1000;
        short *buf = (short *)calloc((size_t)frames, sizeof(short));
        if (buf) {
            float phase = 0.0f;
            int split   = frames / 2;
            for (int i = 0; i < frames; i++) {
                float local_t = (float)(i < split ? i : (i - split)) / (float)AUDIO_SAMPLE_RATE;
                float f       = (i < split) ? 523.25f : 783.99f;
                float env     = expf(-local_t * 22.0f);
                phase        += TWO_PI * f / (float)AUDIO_SAMPLE_RATE;
                float s       = (sinf(phase) + 0.25f * sinf(phase * 2.0f)) * env * 0.36f;
                buf[i]        = (short)(s * 32767.0f);
            }
            g_sfx[SFX_COMPILE_CHIME] = build_procedural_sound(buf, frames);
            free(buf);
        }
    }

    /* 4. SFX_ERROR_THUD: Low dry thud (~145ms, 135Hz -> 62Hz) */
    {
        int frames = (AUDIO_SAMPLE_RATE * 145) / 1000;
        short *buf = (short *)calloc((size_t)frames, sizeof(short));
        if (buf) {
            float phase = 0.0f;
            for (int i = 0; i < frames; i++) {
                float t   = (float)i / (float)AUDIO_SAMPLE_RATE;
                float env = expf(-t * 26.0f);
                float f   = 62.0f + 78.0f * expf(-t * 28.0f);
                phase    += TWO_PI * f / (float)AUDIO_SAMPLE_RATE;
                float raw = sinf(phase) + 0.35f * sinf(phase * 1.5f);
                float s   = tanhf(raw * 1.4f) * env * 0.55f;
                buf[i]    = (short)(s * 32767.0f);
            }
            g_sfx[SFX_ERROR_THUD] = build_procedural_sound(buf, frames);
            free(buf);
        }
    }

    /* 5. SFX_UI_CLICK: Crisp indie UI button click (~12ms) */
    {
        int frames = (AUDIO_SAMPLE_RATE * 12) / 1000;
        short buf[600];
        float phase = 0.0f;
        for (int i = 0; i < frames && i < 600; i++) {
            float t   = (float)i / (float)AUDIO_SAMPLE_RATE;
            float env = expf(-t * 340.0f);
            float f   = 480.0f + 960.0f * expf(-t * 400.0f);
            phase    += TWO_PI * f / (float)AUDIO_SAMPLE_RATE;
            float s   = sinf(phase) * env * 0.38f;
            buf[i]    = (short)(s * 32767.0f);
        }
        g_sfx[SFX_UI_CLICK] = build_procedural_sound(buf, frames);
    }
}

void audio_engine_cleanup(void)
{
    if (!g_audio_ready) return;
    for (int i = 0; i < SFX_COUNT; i++) {
        if (g_sfx[i].frameCount > 0) {
            UnloadSound(g_sfx[i]);
        }
    }
    memset(g_sfx, 0, sizeof(g_sfx));
    CloseAudioDevice();
    g_audio_ready = false;
}

bool audio_engine_is_ready(void)
{
    return g_audio_ready && IsAudioDeviceReady();
}

void audio_play_sfx(SfxType type, const AppConfig *cfg, float pitch_variation)
{
    if (!g_audio_ready || !cfg || !cfg->audio_enabled) return;
    if (type < 0 || type >= SFX_COUNT) return;
    if (cfg->audio_volume <= 0.001f) return;
    if (g_sfx[type].frameCount == 0) return;

    float vol = cfg->audio_volume;
    if (vol > 1.0f) vol = 1.0f;
    float pitch = 1.0f + pitch_variation;
    if (pitch < 0.65f) pitch = 0.65f;
    if (pitch > 1.65f) pitch = 1.65f;

    SetSoundVolume(g_sfx[type], vol);
    SetSoundPitch(g_sfx[type], pitch);
    PlaySound(g_sfx[type]);
}
