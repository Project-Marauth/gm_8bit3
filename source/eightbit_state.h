#pragma once

#include "audio_effects.h"
#include "opus_framedecoder.h"
#include "voice_presets.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct EffectEncoderStream {
	EffectEncoderStream() : codec(new SteamOpus::Opus_FrameDecoder()) {
		codec->Init(5, 24000);
	}

	std::unique_ptr<SteamOpus::Opus_FrameDecoder> codec;
	std::vector<AudioEffects::RuntimeState> stages;
	std::vector<AudioEffects::Preset> chain;
	uint32_t chainGeneration = 0;
	std::chrono::steady_clock::time_point lastUsed;
	bool bUsed = false;
};

struct SpeakerVoiceState {
	SpeakerVoiceState() : decoder(new SteamOpus::Opus_FrameDecoder()) {
		decoder->Init(5, 24000);
	}

	int defaultEffect = AudioEffects::EFF_NONE;
	std::unique_ptr<SteamOpus::Opus_FrameDecoder> decoder;
	std::unordered_map<int, std::unique_ptr<EffectEncoderStream>> encoders;
};

struct EightbitState {
	static uint64_t RouteKey(int listenerUserId, int speakerUserId) {
		return (static_cast<uint64_t>(static_cast<uint32_t>(listenerUserId)) << 32)
			| static_cast<uint32_t>(speakerUserId);
	}

	int crushFactor = 350;
	float gainFactor = 1.2f;
	bool broadcastPackets = false;
	int desampleRate = 2;
	uint16_t port = 4000;
	std::string ip = "127.0.0.1";

	std::unordered_map<int, std::unique_ptr<SpeakerVoiceState>> speakers;
	std::unordered_map<uint64_t, int> receiverEffects;
	std::unordered_map<int, int> routesPerSpeaker;
	uint32_t presetGeneration = 1;
	VoicePresetRegistry presets;
};
