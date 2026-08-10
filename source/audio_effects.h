#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace AudioEffects {

enum EffectId {
	EFF_NONE = 0,
	EFF_BITCRUSH = 1,
	EFF_DESAMPLE = 2,
	EFF_COMB = 3,
	EFF_DARTHVADER = 4,
	EFF_RADIO = 5,
	EFF_ROBOT = 6,
	EFF_ALIEN = 7,
	EFF_OVERDRIVE = 8,
	EFF_DISTORTION = 9,
	EFF_TELEPHONE = 10,
	EFF_MEGAPHONE = 11,
	EFF_CHIPMUNK = 12,
	EFF_SLOWMOTION = 13,
	EFF_COMBO = 14,
	CUSTOM_ID_MIN = 1000,
	CUSTOM_ID_MAX = 65534
};

struct Preset {
	int id = EFF_NONE;
	std::string key;
	std::string name;
	std::string description;
	std::string sourceFile;
	bool custom = false;

	float gain = 1.0f;
	float highpassHz = 0.0f;
	float lowpassHz = 0.0f;
	float drive = 1.0f;
	float clip = 1.0f;
	float pitch = 1.0f;
	float ringFrequency = 0.0f;
	float ringMix = 0.0f;
	float echoDelayMs = 0.0f;
	float echoMix = 0.0f;
	float noise = 0.0f;
	int quantize = 0;
	int downsample = 1;
	bool softClip = false;
};

struct RuntimeState {
	float highpassInput = 0.0f;
	float highpassOutput = 0.0f;
	float lowpassOutput = 0.0f;
	float ringPhase = 0.0f;
	float pitchPhase = 0.0f;
	int heldSample = 0;
	int holdRemaining = 0;
	uint32_t randomState = 0x8B17C3A5u;
	std::vector<int16_t> delay;
	size_t delayPosition = 0;
};

inline int16_t Clamp16(float value) {
	if (value > 32767.0f) return 32767;
	if (value < -32768.0f) return -32768;
	return static_cast<int16_t>(value);
}

inline float ClampFloat(float value, float low, float high) {
	return std::max(low, std::min(high, value));
}

inline Preset MakeBuiltin(int id, float legacyGain, int legacyCrush, int legacyDesample) {
	Preset p;
	p.id = id;
	p.custom = false;

	switch (id) {
	case EFF_NONE:
		p.key = "none"; p.name = "No effect"; p.description = "Unmodified voice";
		break;
	case EFF_BITCRUSH:
		p.key = "bitcrush"; p.name = "Bit crusher"; p.description = "Driven and quantized digital voice";
		p.gain = legacyGain; p.drive = 2.4f; p.clip = 0.78f; p.quantize = std::max(8, legacyCrush); p.softClip = true;
		break;
	case EFF_DESAMPLE:
		p.key = "desample"; p.name = "Desampling"; p.description = "Low fidelity radio-like resampling";
		p.highpassHz = 700.0f; p.lowpassHz = 3600.0f; p.downsample = std::max(1, legacyDesample); p.quantize = 220;
		break;
	case EFF_COMB:
		p.key = "combine"; p.name = "Combine"; p.description = "Dark metallic Combine voice";
		p.pitch = 0.78f; p.highpassHz = 250.0f; p.lowpassHz = 2700.0f; p.drive = 1.8f; p.clip = 0.72f;
		p.ringFrequency = 150.0f; p.ringMix = 0.38f; p.echoDelayMs = 2.1f; p.echoMix = 0.28f; p.quantize = 150;
		break;
	case EFF_DARTHVADER:
		p.key = "darth_vader"; p.name = "Darth Vader"; p.description = "Deep resonant voice";
		p.pitch = 0.70f; p.lowpassHz = 2400.0f; p.ringFrequency = 72.0f; p.ringMix = 0.16f;
		p.echoDelayMs = 4.0f; p.echoMix = 0.18f; p.drive = 1.35f; p.softClip = true;
		break;
	case EFF_RADIO:
		p.key = "radio"; p.name = "Radio"; p.description = "Narrow-band walkie-talkie voice";
		p.highpassHz = 650.0f; p.lowpassHz = 3500.0f; p.drive = 1.35f; p.clip = 0.82f;
		p.quantize = 320; p.downsample = 2; p.noise = 0.008f; p.echoDelayMs = 1.2f; p.echoMix = 0.12f;
		break;
	case EFF_ROBOT:
		p.key = "robot"; p.name = "Robot"; p.description = "Synthetic ring-modulated voice";
		p.pitch = 0.76f; p.highpassHz = 180.0f; p.lowpassHz = 4200.0f; p.ringFrequency = 108.0f;
		p.ringMix = 0.62f; p.echoDelayMs = 3.0f; p.echoMix = 0.34f; p.drive = 1.4f; p.softClip = true;
		break;
	case EFF_ALIEN:
		p.key = "alien"; p.name = "Alien"; p.description = "High and unstable extraterrestrial voice";
		p.pitch = 1.32f; p.highpassHz = 550.0f; p.ringFrequency = 11.0f; p.ringMix = 0.22f;
		p.echoDelayMs = 0.8f; p.echoMix = 0.18f;
		break;
	case EFF_OVERDRIVE:
		p.key = "overdrive"; p.name = "Overdrive"; p.description = "Warm saturated amplifier";
		p.drive = 5.0f; p.gain = 0.92f; p.softClip = true; p.echoDelayMs = 0.5f; p.echoMix = 0.16f;
		break;
	case EFF_DISTORTION:
		p.key = "distortion"; p.name = "Distortion"; p.description = "Hard clipped distortion";
		p.drive = 5.2f; p.clip = 0.68f; p.softClip = false;
		break;
	case EFF_TELEPHONE:
		p.key = "telephone"; p.name = "Telephone"; p.description = "Telephone band-limited voice";
		p.highpassHz = 500.0f; p.lowpassHz = 3200.0f; p.drive = 1.25f; p.clip = 0.86f;
		p.quantize = 380; p.downsample = 2; p.noise = 0.004f; p.echoDelayMs = 0.7f; p.echoMix = 0.13f;
		break;
	case EFF_MEGAPHONE:
		p.key = "megaphone"; p.name = "Megaphone"; p.description = "Public-address loudspeaker voice";
		p.highpassHz = 620.0f; p.lowpassHz = 2700.0f; p.drive = 3.0f; p.clip = 0.72f;
		p.echoDelayMs = 1.0f; p.echoMix = 0.28f; p.quantize = 100;
		break;
	case EFF_CHIPMUNK:
		p.key = "chipmunk"; p.name = "Chipmunk"; p.description = "Raised pitch voice";
		p.pitch = 1.40f; p.highpassHz = 380.0f;
		break;
	case EFF_SLOWMOTION:
		p.key = "slow_motion"; p.name = "Slow motion"; p.description = "Lowered pitch voice";
		p.pitch = 0.68f; p.lowpassHz = 3600.0f;
		break;
	case EFF_COMBO:
		p.key = "metrocop"; p.name = "Metrocop"; p.description = "Civil Protection combined effect";
		p.pitch = 0.84f; p.highpassHz = 300.0f; p.lowpassHz = 3500.0f; p.drive = 1.65f; p.clip = 0.76f;
		p.ringFrequency = 300.0f; p.ringMix = 0.34f; p.echoDelayMs = 1.9f; p.echoMix = 0.30f; p.quantize = 180;
		break;
	default:
		p.id = EFF_NONE; p.key = "none"; p.name = "No effect"; p.description = "Unmodified voice";
		break;
	}

	return p;
}

inline void ApplyPitch(int16_t* audio, int samples, float ratio, RuntimeState& state) {
	if (samples < 2 || std::fabs(ratio - 1.0f) < 0.001f) return;

	std::vector<int16_t> original(audio, audio + samples);
	float position = state.pitchPhase;
	for (int i = 0; i < samples; ++i) {
		while (position >= static_cast<float>(samples - 1)) position -= static_cast<float>(samples - 1);
		const int a = static_cast<int>(position);
		const int b = std::min(a + 1, samples - 1);
		const float fraction = position - static_cast<float>(a);
		audio[i] = Clamp16(original[a] * (1.0f - fraction) + original[b] * fraction);
		position += ratio;
	}
	state.pitchPhase = std::fmod(position, static_cast<float>(samples - 1));
}

inline void ApplyPreset(const Preset& preset, int16_t* audio, int samples, int sampleRate, RuntimeState& state) {
	if (audio == nullptr || samples <= 0 || preset.id == EFF_NONE) return;
	const float pi = 3.14159265358979323846f;

	ApplyPitch(audio, samples, ClampFloat(preset.pitch, 0.5f, 2.0f), state);

	if (preset.highpassHz > 0.0f) {
		const float cutoff = ClampFloat(preset.highpassHz, 10.0f, sampleRate * 0.45f);
		const float rc = 1.0f / (2.0f * pi * cutoff);
		const float dt = 1.0f / static_cast<float>(sampleRate);
		const float alpha = rc / (rc + dt);
		for (int i = 0; i < samples; ++i) {
			const float input = static_cast<float>(audio[i]);
			state.highpassOutput = alpha * (state.highpassOutput + input - state.highpassInput);
			state.highpassInput = input;
			audio[i] = Clamp16(state.highpassOutput);
		}
	}

	if (preset.lowpassHz > 0.0f) {
		const float cutoff = ClampFloat(preset.lowpassHz, 20.0f, sampleRate * 0.48f);
		const float rc = 1.0f / (2.0f * pi * cutoff);
		const float dt = 1.0f / static_cast<float>(sampleRate);
		const float alpha = dt / (rc + dt);
		for (int i = 0; i < samples; ++i) {
			state.lowpassOutput += alpha * (static_cast<float>(audio[i]) - state.lowpassOutput);
			audio[i] = Clamp16(state.lowpassOutput);
		}
	}

	if (preset.ringFrequency > 0.0f && preset.ringMix > 0.0f) {
		const float mix = ClampFloat(preset.ringMix, 0.0f, 1.0f);
		const float step = 2.0f * pi * preset.ringFrequency / static_cast<float>(sampleRate);
		for (int i = 0; i < samples; ++i) {
			const float dry = static_cast<float>(audio[i]);
			const float modulated = dry * std::sin(state.ringPhase);
			audio[i] = Clamp16(dry * (1.0f - mix) + modulated * mix);
			state.ringPhase += step;
			if (state.ringPhase >= 2.0f * pi) state.ringPhase -= 2.0f * pi;
		}
	}

	const int downsample = std::max(1, std::min(32, preset.downsample));
	if (downsample > 1) {
		for (int i = 0; i < samples; ++i) {
			if (state.holdRemaining <= 0) {
				state.heldSample = audio[i];
				state.holdRemaining = downsample;
			}
			audio[i] = static_cast<int16_t>(state.heldSample);
			--state.holdRemaining;
		}
	}

	if (preset.drive > 1.001f || preset.clip < 0.999f) {
		const float drive = ClampFloat(preset.drive, 0.05f, 16.0f);
		const float clip = ClampFloat(preset.clip, 0.05f, 1.0f);
		for (int i = 0; i < samples; ++i) {
			float normalized = static_cast<float>(audio[i]) / 32768.0f;
			normalized *= drive;
			if (preset.softClip) normalized = std::tanh(normalized);
			normalized = ClampFloat(normalized, -clip, clip);
			audio[i] = Clamp16(normalized * 32767.0f);
		}
	}

	if (preset.quantize > 1) {
		const int step = std::min(32767, preset.quantize);
		for (int i = 0; i < samples; ++i) {
			const int value = audio[i];
			audio[i] = static_cast<int16_t>((value / step) * step);
		}
	}

	if (preset.echoDelayMs > 0.0f && preset.echoMix > 0.0f) {
		const int wantedDelay = std::max(1, std::min(sampleRate / 2,
			static_cast<int>(preset.echoDelayMs * static_cast<float>(sampleRate) / 1000.0f)));
		if (state.delay.size() != static_cast<size_t>(wantedDelay)) {
			state.delay.assign(static_cast<size_t>(wantedDelay), 0);
			state.delayPosition = 0;
		}
		const float mix = ClampFloat(preset.echoMix, 0.0f, 0.95f);
		for (int i = 0; i < samples; ++i) {
			const int16_t delayed = state.delay[state.delayPosition];
			const int16_t dry = audio[i];
			audio[i] = Clamp16(static_cast<float>(dry) + static_cast<float>(delayed) * mix);
			state.delay[state.delayPosition] = dry;
			state.delayPosition = (state.delayPosition + 1) % state.delay.size();
		}
	}

	const float noiseAmount = ClampFloat(preset.noise, 0.0f, 0.25f);
	const float gain = ClampFloat(preset.gain, 0.0f, 8.0f);
	if (noiseAmount > 0.0f || std::fabs(gain - 1.0f) > 0.001f) {
		for (int i = 0; i < samples; ++i) {
			state.randomState ^= state.randomState << 13;
			state.randomState ^= state.randomState >> 17;
			state.randomState ^= state.randomState << 5;
			const float noise = (static_cast<float>(state.randomState & 0xFFFFu) / 32767.5f - 1.0f)
				* noiseAmount * 32767.0f;
			audio[i] = Clamp16(static_cast<float>(audio[i]) * gain + noise);
		}
	}
}

} // namespace AudioEffects
