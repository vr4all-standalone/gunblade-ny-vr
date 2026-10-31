// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

// Plain C ABI between the SDL engine and the separately owned audio worker.
// Native emulation/synthesis and the original WAV recorder remain upstream.
inline constexpr uint32_t GunbladeAudioAbi = 0x47424101;
enum GunbladeAudioState : uint32_t {
    GunbladeAudioWarmup = 0, GunbladeAudioRunning = 1,
    GunbladeAudioPaused = 2, GunbladeAudioFailed = 3
};
struct GunbladeAudioStats {
    uint32_t size{},abi{},state{},sample_rate{},channels{};
    uint32_t input_queue_frames{},output_queue_frames{},reserved{};
    uint64_t native_frames{},output_frames{},requested_frames{},delivered_frames{};
    uint64_t missing_frames{},warmup_frames{},paused_frames{},paused_input_frames{};
    uint64_t input_dropped_frames{},output_dropped_frames{};
    uint64_t callbacks{},underrun_callbacks{},input_blocks{},output_blocks{};
    double tempo{1},input_ratio{1},max_input_gap_ms{},max_output_gap_ms{};
};

// Push/read timestamps use the same caller-supplied monotonic seconds.
// Push consumes/copies input without DSP work. Intentional paused input is
// counted separately from overflow. Read always fills the destination, returns
// real delivered frames, and zero-fills any remainder. It performs no locking,
// allocation or I/O. Missing includes warmup but excludes intentional pause:
// requested = delivered + missing + paused; warmup is a subset of missing.
// Pause only stores control state; DSP reset/drain occurs on worker/consumer.
// Stats are cumulative atomic observations, not a globally latched snapshot.
#if defined(_WIN32) && defined(GUNBLADE_AUDIO_BUILD)
#define GUNBLADE_AUDIO_EXPORT extern "C" __declspec(dllexport)
#elif defined(GUNBLADE_AUDIO_BUILD)
#define GUNBLADE_AUDIO_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define GUNBLADE_AUDIO_EXPORT extern "C"
#endif
GUNBLADE_AUDIO_EXPORT void* gunblade_audio_create(uint32_t rate,uint32_t channels);
GUNBLADE_AUDIO_EXPORT uint32_t gunblade_audio_push(void*,const int16_t*,uint32_t frames,double now);
GUNBLADE_AUDIO_EXPORT uint32_t gunblade_audio_read(void*,int16_t*,uint32_t frames,double now);
GUNBLADE_AUDIO_EXPORT void gunblade_audio_pause(void*,uint32_t paused);
GUNBLADE_AUDIO_EXPORT uint32_t gunblade_audio_stats(void*,GunbladeAudioStats*,uint32_t bytes);
GUNBLADE_AUDIO_EXPORT void gunblade_audio_destroy(void*);
