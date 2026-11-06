#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
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
	EFF_METROCOP_MODULATOR = 15,
	EFF_OVERWATCH_MODULATOR = 16,
	EFF_COMBINE_DISPATCH = 17,
	EFF_GAS_MASK = 18,
	EFF_CLOTH_MASK = 19,
	EFF_SEALED_HELMET = 20,
	EFF_WALKIE_TALKIE = 21,
	EFF_WEAK_SIGNAL = 22,
	EFF_LOUDHAILER = 23,
	EFF_PA_SYSTEM = 24,
	EFF_THROUGH_WALL = 25,
	EFF_UNDERWATER = 26,
	EFF_COMBINE_GRUNT = 27,
	EFF_BUILTIN_LAST = EFF_COMBINE_GRUNT,
	CUSTOM_ID_MIN = 1000,
	CUSTOM_ID_MAX = 65534
};

/// Modifier layers. An effect id is a base preset in the low 16 bits plus any number of layer flags above them, so a masked Combine officer keying a radio is heard through the modulator, the mask and the radio at once instead of one preset replacing the other. Every base id, built-in or custom, fits under EFF_BASE_MASK, so passing a bare preset id is unaffected.
enum EffectFlag {
	EFF_BASE_MASK = 0x0000FFFF,
	EFF_FLAG_CLOTH = 1 << 16,
	EFF_FLAG_MUFFLE = 1 << 17,
	EFF_FLAG_HELMET = 1 << 18,
	EFF_FLAG_MEGAPHONE = 1 << 19,
	EFF_FLAG_RADIO = 1 << 20,
	EFF_FLAG_TELEPHONE = 1 << 21,
	EFF_FLAG_INTERCOM = 1 << 22,
	EFF_FLAG_WEAK_SIGNAL = 1 << 23,
	EFF_FLAG_WALL = 1 << 24,
	EFF_FLAG_UNDERWATER = 1 << 25,
	EFF_FLAG_MASK = 0x03FF0000
};

/// A layer flag and the built-in preset it runs. The table is in signal-chain order: what the mouth passes through first (cloth, mask, helmet), then the device that carries it (megaphone, radio, telephone, intercom), then the channel (a weak signal), then the space between the device and the listener (a wall, water).
struct LayerInfo {
	int flag;
	int presetId;
	const char* key;
	const char* name;
};

inline const LayerInfo* Layers(int& count) {
	static const LayerInfo layers[] = {
		{ EFF_FLAG_CLOTH, EFF_CLOTH_MASK, "cloth", "Cloth over the mouth" },
		{ EFF_FLAG_MUFFLE, EFF_GAS_MASK, "muffle", "Gas mask" },
		{ EFF_FLAG_HELMET, EFF_SEALED_HELMET, "helmet", "Sealed helmet" },
		{ EFF_FLAG_MEGAPHONE, EFF_LOUDHAILER, "megaphone", "Megaphone" },
		{ EFF_FLAG_RADIO, EFF_WALKIE_TALKIE, "radio", "Radio" },
		{ EFF_FLAG_TELEPHONE, EFF_TELEPHONE, "telephone", "Telephone" },
		{ EFF_FLAG_INTERCOM, EFF_PA_SYSTEM, "intercom", "Intercom" },
		{ EFF_FLAG_WEAK_SIGNAL, EFF_WEAK_SIGNAL, "weak_signal", "Weak signal" },
		{ EFF_FLAG_WALL, EFF_THROUGH_WALL, "wall", "Through a wall" },
		{ EFF_FLAG_UNDERWATER, EFF_UNDERWATER, "underwater", "Underwater" }
	};
	count = static_cast<int>(sizeof(layers) / sizeof(layers[0]));
	return layers;
}

constexpr int PEAK_COUNT = 3;

/// Upper bound on vocoder bands. Each band costs two biquads and two envelope followers per sample.
constexpr int VOCODER_MAX_BANDS = 32;

/// Biquads cascaded per vocoder band on each side: a third-order Butterworth bandpass, so each band is sixth order with a flat top. Simpler bands either overlap so much that each hears most of the spectrum or leave gaps between them, and either way words smear into noise (measured: 11% against 36% speech-recognition word errors on the same voices).
constexpr int VOCODER_STAGES = 3;

/// The compressor works out its gain once per this many samples and ramps linearly in between, which keeps the per-sample cost to an envelope follower and a multiply. Must be a power of two.
constexpr int COMPRESSOR_CONTROL_SAMPLES = 16;

/// Resolution of the sin² crossfade table the pitch shifter reads its tap weights from.
constexpr int PITCH_WINDOW_TABLE = 1024;

inline const float* PitchWindowTable() {
	static const std::vector<float> table = [] {
		const float pi = 3.14159265358979323846f;
		std::vector<float> values(PITCH_WINDOW_TABLE + 1);
		for (int i = 0; i <= PITCH_WINDOW_TABLE; ++i) {
			const float weight = std::sin(pi * static_cast<float>(i) / static_cast<float>(PITCH_WINDOW_TABLE));
			values[static_cast<size_t>(i)] = weight * weight;
		}
		return values;
	}();
	return table.data();
}

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

	float bandLowHz = 0.0f;
	float bandHighHz = 0.0f;
	int bandOrder = 2;
	float peakHz[PEAK_COUNT] = { 0.0f, 0.0f, 0.0f };
	float peakDb[PEAK_COUNT] = { 0.0f, 0.0f, 0.0f };
	float peakQ[PEAK_COUNT] = { 1.0f, 1.0f, 1.0f };
	float tubeMs = 0.0f;
	float tubeFeedback = 0.0f;
	float tubeMix = 0.0f;
	float compressThresholdDb = 0.0f;
	float compressRatio = 1.0f;
	float compressMakeupDb = 0.0f;
	float hiss = 0.0f;
	float crackle = 0.0f;
	float dropout = 0.0f;
	float squelchMs = 0.0f;
	float squelchLevel = 0.0f;
	float chirpHz = 0.0f;
	float chirpMs = 0.0f;
	float chirpLevel = 0.0f;
	int chirpCount = 1;
	float reverbMix = 0.0f;
	float reverbSize = 0.5f;
	float reverbDamp = 0.5f;
	float wobbleHz = 0.0f;
	float wobbleMs = 0.0f;
	bool limit = false;

	float vocoderMix = 0.0f;
	int vocoderBands = 16;
	float vocoderLowHz = 150.0f;
	float vocoderHighHz = 4000.0f;
	float vocoderWidth = 1.0f;
	float vocoderAttackMs = 2.0f;
	float vocoderReleaseMs = 20.0f;
	float vocoderNoise = 0.0f;
	float vocoderBuzz = 1.0f;
	float vocoderSelf = 0.0f;
	float vocoderBuzzHz = 110.0f;
	float vocoderGain = 1.0f;
	float vocoderFormant = 1.0f;
	float vocoderFlatten = 0.0f;
	float vocoderSmooth = 0.0f;
};

struct Biquad {
	float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
	float z1 = 0.0f, z2 = 0.0f;

	float Process(float input) {
		const float output = b0 * input + z1;
		z1 = b1 * input - a1 * output + z2;
		z2 = b2 * input - a2 * output;
		return output;
	}
};

/// A sine oscillator advanced by complex rotation instead of a `sin` call per sample. It is seeded from a stored phase at the start of each block, so rounding drift never outlives one packet.
struct Phasor {
	Phasor(float phase, float step)
		: sine(std::sin(phase)), cosine(std::cos(phase)), stepSine(std::sin(step)), stepCosine(std::cos(step)) {}

	float Next() {
		const float value = sine;
		const float nextSine = sine * stepCosine + cosine * stepSine;
		cosine = cosine * stepCosine - sine * stepSine;
		sine = nextSine;
		return value;
	}

	float sine, cosine, stepSine, stepCosine;
};

struct ReverbComb {
	std::vector<float> buffer;
	size_t position = 0;
	float store = 0.0f;
};

struct ReverbAllpass {
	std::vector<float> buffer;
	size_t position = 0;
};

struct RuntimeState {
	float highpassInput = 0.0f;
	float highpassOutput = 0.0f;
	float lowpassOutput = 0.0f;
	float ringPhase = 0.0f;
	int heldSample = 0;
	int holdRemaining = 0;
	uint32_t randomState = 0x8B17C3A5u;
	std::vector<float> delay;
	size_t delayPosition = 0;

	std::vector<float> pitchBuffer;
	size_t pitchWrite = 0;
	float pitchDelay = 0.0f;

	std::vector<float> wobbleBuffer;
	size_t wobbleWrite = 0;
	float wobblePhase = 0.0f;

	std::vector<float> tubeBuffer;
	size_t tubePosition = 0;

	Biquad bandLow[2];
	Biquad bandHigh[2];
	Biquad peaks[PEAK_COUNT];
	Biquad noiseShape;

	Biquad vocoderAnalysis[VOCODER_MAX_BANDS][VOCODER_STAGES];
	Biquad vocoderSynthesis[VOCODER_MAX_BANDS][VOCODER_STAGES];
	float vocoderVoiceEnvelope[VOCODER_MAX_BANDS] = {};
	float vocoderCarrierEnvelope[VOCODER_MAX_BANDS] = {};
	float vocoderBuzzPhase = 0.0f;
	float vocoderLongTerm[VOCODER_MAX_BANDS] = {};
	float vocoderDesign[6] = {};

	float compressorEnvelope = 0.0f;
	float compressorGain = 1.0f;
	float compressorGainStep = 0.0f;
	float crackleLevel = 0.0f;
	int dropoutRemaining = 0;
	float dropoutGain = 1.0f;

	ReverbComb reverbCombs[4];
	ReverbAllpass reverbAllpasses[2];

	bool transmissionStart = false;
	int squelchRemaining = 0;
	int squelchTotal = 0;
	int chirpPosition = -1;
	float chirpPhase = 0.0f;
};

inline int16_t Clamp16(float value) {
	if (value > 32767.0f) return 32767;
	if (value < -32768.0f) return -32768;
	return static_cast<int16_t>(value);
}

inline float ClampFloat(float value, float low, float high) {
	return std::max(low, std::min(high, value));
}

inline float ClampSample(float value) {
	return ClampFloat(value, -32768.0f, 32767.0f);
}

/// Clears every filter, delay line and envelope back to silence while keeping the noise generator running, so the start of a new transmission never replays the tail of the last one out of a delay buffer.
inline void ResetRuntime(RuntimeState& state) {
	const uint32_t seed = state.randomState;
	state = RuntimeState();
	state.randomState = seed;
}

/// Marks the next processed block as the first of a transmission: the state is cleared and any squelch burst or key-up chirp the preset declares plays over its opening milliseconds.
inline void BeginTransmission(RuntimeState& state) {
	ResetRuntime(state);
	state.transmissionStart = true;
}

inline float NextRandom(RuntimeState& state) {
	state.randomState ^= state.randomState << 13;
	state.randomState ^= state.randomState >> 17;
	state.randomState ^= state.randomState << 5;
	return static_cast<float>(state.randomState & 0xFFFFu) / 32767.5f - 1.0f;
}

inline float NextUnit(RuntimeState& state) {
	return (NextRandom(state) + 1.0f) * 0.5f;
}

inline void SetLowpass(Biquad& filter, float hz, float q, int sampleRate) {
	const float pi = 3.14159265358979323846f;
	const float w0 = 2.0f * pi * ClampFloat(hz, 20.0f, sampleRate * 0.45f) / static_cast<float>(sampleRate);
	const float cosw = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * q);
	const float a0 = 1.0f + alpha;
	filter.b0 = ((1.0f - cosw) * 0.5f) / a0;
	filter.b1 = (1.0f - cosw) / a0;
	filter.b2 = filter.b0;
	filter.a1 = (-2.0f * cosw) / a0;
	filter.a2 = (1.0f - alpha) / a0;
}

inline void SetHighpass(Biquad& filter, float hz, float q, int sampleRate) {
	const float pi = 3.14159265358979323846f;
	const float w0 = 2.0f * pi * ClampFloat(hz, 10.0f, sampleRate * 0.45f) / static_cast<float>(sampleRate);
	const float cosw = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * q);
	const float a0 = 1.0f + alpha;
	filter.b0 = ((1.0f + cosw) * 0.5f) / a0;
	filter.b1 = -(1.0f + cosw) / a0;
	filter.b2 = filter.b0;
	filter.a1 = (-2.0f * cosw) / a0;
	filter.a2 = (1.0f - alpha) / a0;
}

inline void SetPeaking(Biquad& filter, float hz, float db, float q, int sampleRate) {
	const float pi = 3.14159265358979323846f;
	const float w0 = 2.0f * pi * ClampFloat(hz, 20.0f, sampleRate * 0.45f) / static_cast<float>(sampleRate);
	const float cosw = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * ClampFloat(q, 0.1f, 24.0f));
	const float a = std::pow(10.0f, db / 40.0f);
	const float a0 = 1.0f + alpha / a;
	filter.b0 = (1.0f + alpha * a) / a0;
	filter.b1 = (-2.0f * cosw) / a0;
	filter.b2 = (1.0f - alpha * a) / a0;
	filter.a1 = (-2.0f * cosw) / a0;
	filter.a2 = (1.0f - alpha / a) / a0;
}

inline void SetBandpass(Biquad& filter, float hz, float q, int sampleRate) {
	const float pi = 3.14159265358979323846f;
	const float w0 = 2.0f * pi * ClampFloat(hz, 20.0f, sampleRate * 0.45f) / static_cast<float>(sampleRate);
	const float cosw = std::cos(w0);
	const float alpha = std::sin(w0) / (2.0f * q);
	const float a0 = 1.0f + alpha;
	filter.b0 = alpha / a0;
	filter.b1 = 0.0f;
	filter.b2 = -alpha / a0;
	filter.a1 = (-2.0f * cosw) / a0;
	filter.a2 = (1.0f - alpha) / a0;
}

/// Designs a Butterworth bandpass between two edges as VOCODER_STAGES biquads: the low-pass prototype's poles are moved to the band by the standard low-pass to bandpass transform, mapped through the bilinear transform with prewarped edges, and the cascade is normalised to unity gain at the band's centre.
inline void DesignButterworthBandpass(Biquad* sections, float lowHz, float highHz, int sampleRate) {
	const double pi = 3.14159265358979323846;
	const double fs = static_cast<double>(sampleRate);
	const double low = 2.0 * fs * std::tan(pi * lowHz / fs);
	const double high = 2.0 * fs * std::tan(pi * highHz / fs);
	const double centre = std::sqrt(low * high);
	const double width = high - low;
	const int order = VOCODER_STAGES;

	std::complex<double> poles[VOCODER_STAGES];
	int count = 0;
	for (int k = 0; k < order; ++k) {
		const std::complex<double> prototype = std::polar(1.0, pi * (2.0 * k + order + 1) / (2.0 * order));
		const std::complex<double> half = prototype * width * 0.5;
		const std::complex<double> root = std::sqrt(half * half - centre * centre);
		for (const std::complex<double>& pole : { half + root, half - root }) {
			if (pole.imag() > 0.0 && count < VOCODER_STAGES) poles[count++] = pole;
		}
	}

	const double centreDigital = 2.0 * pi * std::sqrt(static_cast<double>(lowHz) * highHz) / fs;
	const std::complex<double> unit = std::polar(1.0, -centreDigital);
	std::complex<double> response = 1.0;
	for (int i = 0; i < count; ++i) {
		const std::complex<double> z = (2.0 * fs + poles[i]) / (2.0 * fs - poles[i]);
		Biquad& section = sections[i];
		section.b0 = 1.0f;
		section.b1 = 0.0f;
		section.b2 = -1.0f;
		section.a1 = static_cast<float>(-2.0 * z.real());
		section.a2 = static_cast<float>(std::norm(z));
		response *= (1.0 - unit * unit) / (1.0 + static_cast<double>(section.a1) * unit + static_cast<double>(section.a2) * unit * unit);
	}

	const float scale = static_cast<float>(std::pow(1.0 / std::max(1e-12, std::abs(response)), 1.0 / std::max(1, count)));
	for (int i = 0; i < count; ++i) {
		sections[i].b0 = scale;
		sections[i].b2 = -scale;
	}
}

inline float ReadFractional(const std::vector<float>& buffer, size_t write, float delay) {
	const size_t size = buffer.size();
	const float position = static_cast<float>(write) - delay + static_cast<float>(size);
	const size_t whole = static_cast<size_t>(position);
	const float fraction = position - static_cast<float>(whole);
	const float a = buffer[whole % size];
	const float b = buffer[(whole + 1) % size];
	return a + (b - a) * fraction;
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
	case EFF_METROCOP_MODULATOR:
		p.key = "metrocop_modulator"; p.name = "Metrocop modulator";
		p.description = "Civil Protection mask: a 12-band vocoder that replaces the officer's pitch, timbre and formants, respirator honk, radio band, static on key-up";
		p.vocoderMix = 1.0f; p.vocoderBands = 12; p.vocoderLowHz = 150.0f; p.vocoderHighHz = 4200.0f;
		p.vocoderNoise = 0.45f; p.vocoderBuzz = 0.55f; p.vocoderSelf = 0.0f; p.vocoderBuzzHz = 96.0f;
		p.vocoderAttackMs = 8.0f; p.vocoderReleaseMs = 30.0f;
		p.vocoderFormant = 1.18f; p.vocoderFlatten = 1.0f; p.vocoderSmooth = 0.5f;
		p.tubeMs = 0.75f; p.tubeFeedback = 0.42f; p.tubeMix = 0.45f;
		p.drive = 2.2f; p.softClip = true; p.quantize = 48;
		p.compressThresholdDb = -26.0f; p.compressRatio = 6.0f; p.compressMakeupDb = 9.0f;
		p.hiss = 0.006f; p.crackle = 0.12f;
		p.bandLowHz = 340.0f; p.bandHighHz = 3200.0f; p.bandOrder = 2;
		p.peakHz[0] = 1150.0f; p.peakDb[0] = 7.0f; p.peakQ[0] = 2.2f;
		p.peakHz[1] = 2450.0f; p.peakDb[1] = 4.0f; p.peakQ[1] = 3.0f;
		p.peakHz[2] = 650.0f; p.peakDb[2] = -3.0f; p.peakQ[2] = 1.0f;
		p.squelchMs = 120.0f; p.squelchLevel = 0.09f; p.gain = 1.8f;
		p.limit = true;
		break;
	case EFF_OVERWATCH_MODULATOR:
		p.key = "overwatch_modulator"; p.name = "Overwatch modulator";
		p.description = "Overwatch soldier: a 10-band, mostly unpitched vocoder that leaves nothing of the speaker's own voice, heavily compressed, two-tone chirp on key-up";
		p.vocoderMix = 1.0f; p.vocoderBands = 10; p.vocoderLowHz = 150.0f; p.vocoderHighHz = 4200.0f;
		p.vocoderNoise = 0.65f; p.vocoderBuzz = 0.35f; p.vocoderSelf = 0.0f; p.vocoderBuzzHz = 82.0f;
		p.vocoderAttackMs = 8.0f; p.vocoderReleaseMs = 30.0f;
		p.vocoderFormant = 1.22f; p.vocoderFlatten = 1.0f; p.vocoderSmooth = 0.7f;
		p.peakHz[2] = 2900.0f; p.peakDb[2] = -4.0f; p.peakQ[2] = 1.2f;
		p.ringFrequency = 62.0f; p.ringMix = 0.08f;
		p.tubeMs = 1.4f; p.tubeFeedback = 0.55f; p.tubeMix = 0.5f;
		p.drive = 3.0f; p.softClip = true; p.quantize = 160; p.downsample = 2;
		p.compressThresholdDb = -28.0f; p.compressRatio = 8.0f; p.compressMakeupDb = 11.0f;
		p.hiss = 0.004f;
		p.bandLowHz = 280.0f; p.bandHighHz = 3400.0f; p.bandOrder = 2;
		p.peakHz[0] = 900.0f; p.peakDb[0] = 6.0f; p.peakQ[0] = 2.0f;
		p.peakHz[1] = 1900.0f; p.peakDb[1] = 5.0f; p.peakQ[1] = 2.5f;
		p.squelchMs = 70.0f; p.squelchLevel = 0.05f; p.gain = 1.4f;
		p.chirpHz = 1750.0f; p.chirpMs = 55.0f; p.chirpLevel = 0.05f; p.chirpCount = 2;
		p.limit = true;
		break;
	case EFF_COMBINE_DISPATCH:
		p.key = "combine_dispatch"; p.name = "Combine dispatch";
		p.description = "Overwatch city announcer over loudspeakers, with a long slapback off the buildings";
		p.drive = 1.4f; p.softClip = true;
		p.compressThresholdDb = -24.0f; p.compressRatio = 4.0f; p.compressMakeupDb = 6.0f;
		p.bandLowHz = 160.0f; p.bandHighHz = 5200.0f; p.bandOrder = 2;
		p.peakHz[0] = 2800.0f; p.peakDb[0] = 3.0f; p.peakQ[0] = 1.2f;
		p.echoDelayMs = 140.0f; p.echoMix = 0.18f;
		p.reverbMix = 0.32f; p.reverbSize = 0.85f; p.reverbDamp = 0.4f;
		p.limit = true;
		break;
	case EFF_GAS_MASK:
		p.key = "gas_mask"; p.name = "Gas mask";
		p.description = "Rubber respirator: dulled highs and a hollow cavity resonance";
		p.gain = 0.85f; p.drive = 1.2f; p.softClip = true;
		p.tubeMs = 0.35f; p.tubeFeedback = 0.35f; p.tubeMix = 0.35f;
		p.bandLowHz = 140.0f; p.bandHighHz = 1500.0f; p.bandOrder = 2;
		p.peakHz[0] = 720.0f; p.peakDb[0] = 6.0f; p.peakQ[0] = 1.6f;
		p.peakHz[1] = 1350.0f; p.peakDb[1] = 3.0f; p.peakQ[1] = 3.0f;
		p.limit = true;
		break;
	case EFF_CLOTH_MASK:
		p.key = "cloth_mask"; p.name = "Cloth mask";
		p.description = "Scarf, bandana or surgical mask: slightly softened consonants";
		p.gain = 0.92f; p.bandHighHz = 3200.0f; p.bandOrder = 1;
		p.peakHz[0] = 2600.0f; p.peakDb[0] = -5.0f; p.peakQ[0] = 0.8f;
		break;
	case EFF_SEALED_HELMET:
		p.key = "sealed_helmet"; p.name = "Sealed helmet";
		p.description = "Hazmat hood or sealed visor: boomy, boxed-in and close";
		p.tubeMs = 0.9f; p.tubeFeedback = 0.5f; p.tubeMix = 0.4f;
		p.bandLowHz = 120.0f; p.bandHighHz = 2600.0f; p.bandOrder = 2;
		p.peakHz[0] = 380.0f; p.peakDb[0] = 5.0f; p.peakQ[0] = 1.4f;
		p.peakHz[1] = 1100.0f; p.peakDb[1] = 3.0f; p.peakQ[1] = 2.0f;
		p.reverbMix = 0.12f; p.reverbSize = 0.15f; p.reverbDamp = 0.7f;
		p.limit = true;
		break;
	case EFF_WALKIE_TALKIE:
		p.key = "walkie_talkie"; p.name = "Walkie-talkie";
		p.description = "Handheld FM radio: automatic gain, narrow band, hiss and a squelch burst on key-up";
		p.drive = 2.0f; p.softClip = true; p.clip = 0.9f; p.quantize = 40;
		p.compressThresholdDb = -30.0f; p.compressRatio = 10.0f; p.compressMakeupDb = 13.0f;
		p.hiss = 0.009f; p.crackle = 0.2f;
		p.bandLowHz = 420.0f; p.bandHighHz = 2900.0f; p.bandOrder = 2;
		p.peakHz[0] = 1800.0f; p.peakDb[0] = 4.0f; p.peakQ[0] = 1.5f;
		p.squelchMs = 90.0f; p.squelchLevel = 0.12f; p.gain = 2.0f;
		p.limit = true;
		break;
	case EFF_WEAK_SIGNAL:
		p.key = "weak_signal"; p.name = "Weak signal";
		p.description = "A transmission at the edge of range: heavy static, crackle and the voice dropping out";
		p.hiss = 0.028f; p.crackle = 0.7f; p.dropout = 0.45f; p.gain = 1.6f;
		p.bandLowHz = 500.0f; p.bandHighHz = 2600.0f; p.bandOrder = 1;
		p.limit = true;
		break;
	case EFF_LOUDHAILER:
		p.key = "loudhailer"; p.name = "Loudhailer";
		p.description = "Handheld megaphone: honking horn resonance, driven driver and an outdoor slapback";
		p.drive = 3.4f; p.softClip = true; p.clip = 0.8f; p.gain = 1.5f;
		p.compressThresholdDb = -22.0f; p.compressRatio = 5.0f; p.compressMakeupDb = 6.0f;
		p.bandLowHz = 520.0f; p.bandHighHz = 4200.0f; p.bandOrder = 2;
		p.peakHz[0] = 1700.0f; p.peakDb[0] = 8.0f; p.peakQ[0] = 2.0f;
		p.peakHz[1] = 3100.0f; p.peakDb[1] = 3.0f; p.peakQ[1] = 3.0f;
		p.echoDelayMs = 85.0f; p.echoMix = 0.12f;
		p.reverbMix = 0.1f; p.reverbSize = 0.5f; p.reverbDamp = 0.5f;
		p.limit = true;
		break;
	case EFF_PA_SYSTEM:
		p.key = "pa_system"; p.name = "PA system";
		p.description = "Building intercom or tannoy: ceiling speakers in a large reflective hall";
		p.drive = 1.6f; p.softClip = true;
		p.compressThresholdDb = -24.0f; p.compressRatio = 4.0f; p.compressMakeupDb = 6.0f;
		p.hiss = 0.003f;
		p.bandLowHz = 300.0f; p.bandHighHz = 3800.0f; p.bandOrder = 2;
		p.peakHz[0] = 2200.0f; p.peakDb[0] = 4.0f; p.peakQ[0] = 1.4f;
		p.echoDelayMs = 60.0f; p.echoMix = 0.14f;
		p.reverbMix = 0.38f; p.reverbSize = 0.8f; p.reverbDamp = 0.55f;
		p.limit = true;
		break;
	case EFF_THROUGH_WALL:
		p.key = "through_wall"; p.name = "Through a wall";
		p.description = "A voice in the next room: only the low end gets through";
		p.gain = 0.6f; p.bandLowHz = 70.0f; p.bandHighHz = 520.0f; p.bandOrder = 2;
		p.peakHz[0] = 190.0f; p.peakDb[0] = 4.0f; p.peakQ[0] = 1.0f;
		p.reverbMix = 0.14f; p.reverbSize = 0.3f; p.reverbDamp = 0.85f;
		break;
	case EFF_UNDERWATER:
		p.key = "underwater"; p.name = "Underwater";
		p.description = "Submerged: dark, wavering and swallowed by the water";
		p.gain = 0.75f; p.wobbleHz = 5.5f; p.wobbleMs = 1.6f; p.hiss = 0.004f;
		p.bandHighHz = 650.0f; p.bandOrder = 2;
		p.peakHz[0] = 300.0f; p.peakDb[0] = 5.0f; p.peakQ[0] = 1.2f;
		p.reverbMix = 0.25f; p.reverbSize = 0.6f; p.reverbDamp = 0.9f;
		p.limit = true;
		break;
	case EFF_COMBINE_GRUNT:
		p.key = "combine_grunt"; p.name = "Combine grunt vocoder";
		p.description = "Heavy field vocoder: 8 bands on a low buzz and noise, identity fully replaced, crushed and clipped; hard to understand by design";
		p.vocoderMix = 1.0f; p.vocoderBands = 8; p.vocoderLowHz = 150.0f; p.vocoderHighHz = 4200.0f;
		p.vocoderNoise = 0.5f; p.vocoderBuzz = 0.5f; p.vocoderSelf = 0.0f; p.vocoderBuzzHz = 70.0f;
		p.vocoderAttackMs = 8.0f; p.vocoderReleaseMs = 30.0f; p.vocoderGain = 1.2f;
		p.vocoderFormant = 1.22f; p.vocoderFlatten = 1.0f; p.vocoderSmooth = 0.7f;
		p.drive = 3.6f; p.softClip = true; p.quantize = 120;
		p.compressThresholdDb = -26.0f; p.compressRatio = 8.0f; p.compressMakeupDb = 10.0f;
		p.hiss = 0.005f; p.crackle = 0.1f;
		p.bandLowHz = 220.0f; p.bandHighHz = 3600.0f; p.bandOrder = 2;
		p.peakHz[0] = 1000.0f; p.peakDb[0] = 5.0f; p.peakQ[0] = 1.8f;
		p.peakHz[1] = 2900.0f; p.peakDb[1] = -4.0f; p.peakQ[1] = 1.2f;
		p.squelchMs = 80.0f; p.squelchLevel = 0.06f; p.gain = 1.4f;
		p.limit = true;
		break;
	default:
		p.id = EFF_NONE; p.key = "none"; p.name = "No effect"; p.description = "Unmodified voice";
		break;
	}

	return p;
}

/// Shifts pitch without changing duration through a two-tap delay line whose taps sweep at (1 - ratio) samples per sample and crossfade on a sin² window. The line persists across packets, so the shift is continuous instead of restarting (and clicking) at every packet boundary.
inline void ApplyPitch(float* audio, int samples, float ratio, int sampleRate, RuntimeState& state) {
	if (samples <= 0 || std::fabs(ratio - 1.0f) < 0.001f) return;

	const float window = std::max(256.0f, static_cast<float>(sampleRate) * 0.04f);
	const size_t size = static_cast<size_t>(window) * 2 + 4;
	if (state.pitchBuffer.size() != size) {
		state.pitchBuffer.assign(size, 0.0f);
		state.pitchWrite = 0;
		state.pitchDelay = 0.0f;
	}

	const float* weights = PitchWindowTable();
	const float toTable = static_cast<float>(PITCH_WINDOW_TABLE) / window;
	for (int i = 0; i < samples; ++i) {
		state.pitchBuffer[state.pitchWrite] = audio[i];
		const float first = state.pitchDelay;
		float second = first + window * 0.5f;
		if (second >= window) second -= window;
		const float firstWeight = weights[std::min(PITCH_WINDOW_TABLE, static_cast<int>(first * toTable))];
		const float secondWeight = weights[std::min(PITCH_WINDOW_TABLE, static_cast<int>(second * toTable))];
		audio[i] = ReadFractional(state.pitchBuffer, state.pitchWrite, first) * firstWeight
			+ ReadFractional(state.pitchBuffer, state.pitchWrite, second) * secondWeight;
		state.pitchDelay += 1.0f - ratio;
		if (state.pitchDelay >= window) state.pitchDelay -= window;
		if (state.pitchDelay < 0.0f) state.pitchDelay += window;
		state.pitchWrite = (state.pitchWrite + 1) % size;
	}
}

/// A channel vocoder: the voice is split into log-spaced bands, each band's envelope is followed, and a synthetic carrier split through matching bands is shaped by those envelopes. The carrier blends white noise (unpitched, whispered), a fixed sawtooth buzz (monotone robot) and the voice hard-clipped into a square wave (pitch-following, gritty). Each carrier band is normalised by its own envelope so the output level follows the voice whatever the carrier is.
///
/// Three controls remove what identifies the speaker rather than what they say. `vocoderFormant` re-synthesises every band at a shifted frequency, which moves the formants (the apparent size of the vocal tract) independently of pitch. `vocoderFlatten` divides each band by its own slow average, so a speaker's long-term timbre (nasality, breathiness, chest resonance, even their microphone) is levelled towards a flat template while the fast articulation that carries words stays. `vocoderSmooth` blurs neighbouring bands together, discarding the fine formant detail that tells two voices apart. With a buzz or noise carrier the speaker's pitch never reaches the output at all.
inline void ApplyVocoder(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.vocoderMix <= 0.0f) return;

	const int bands = std::max(4, std::min(VOCODER_MAX_BANDS, preset.vocoderBands));
	const float nyquistLimit = static_cast<float>(sampleRate) * 0.45f;
	const float low = ClampFloat(preset.vocoderLowHz, 40.0f, static_cast<float>(sampleRate) * 0.3f);
	const float high = ClampFloat(preset.vocoderHighHz, low * 1.5f, nyquistLimit);
	const float widthScale = ClampFloat(preset.vocoderWidth, 0.25f, 4.0f);
	const float formant = ClampFloat(preset.vocoderFormant, 0.5f, 2.0f);
	const float design[6] = { static_cast<float>(bands), low, high, widthScale, formant, static_cast<float>(sampleRate) };

	if (!std::equal(design, design + 6, state.vocoderDesign)) {
		std::copy(design, design + 6, state.vocoderDesign);
		const float step = std::pow(high / low, 1.0f / static_cast<float>(bands));
		const float spread = std::pow(step, 0.5f * widthScale);
		for (int b = 0; b < bands; ++b) {
			const float centre = low * std::pow(step, static_cast<float>(b) + 0.5f);
			const float bandLow = ClampFloat(centre / spread, 20.0f, nyquistLimit * 0.95f);
			const float bandHigh = ClampFloat(centre * spread, bandLow * 1.01f, nyquistLimit);
			DesignButterworthBandpass(state.vocoderAnalysis[b], bandLow, bandHigh, sampleRate);
			const float shiftedLow = ClampFloat(bandLow * formant, 20.0f, nyquistLimit * 0.95f);
			const float shiftedHigh = ClampFloat(bandHigh * formant, shiftedLow * 1.01f, nyquistLimit);
			DesignButterworthBandpass(state.vocoderSynthesis[b], shiftedLow, shiftedHigh, sampleRate);
		}
	}

	const float attack = 1.0f - std::exp(-1.0f / (ClampFloat(preset.vocoderAttackMs, 0.1f, 50.0f) * 0.001f * static_cast<float>(sampleRate)));
	const float release = 1.0f - std::exp(-1.0f / (ClampFloat(preset.vocoderReleaseMs, 1.0f, 200.0f) * 0.001f * static_cast<float>(sampleRate)));
	const float carrierFollow = 1.0f - std::exp(-1.0f / (0.01f * static_cast<float>(sampleRate)));
	const float noiseWeight = ClampFloat(preset.vocoderNoise, 0.0f, 1.0f);
	const float buzzWeight = ClampFloat(preset.vocoderBuzz, 0.0f, 1.0f);
	const float selfWeight = ClampFloat(preset.vocoderSelf, 0.0f, 1.0f);
	const float buzzStep = ClampFloat(preset.vocoderBuzzHz, 20.0f, 1000.0f) / static_cast<float>(sampleRate);
	const float mix = ClampFloat(preset.vocoderMix, 0.0f, 1.0f);
	const float gain = ClampFloat(preset.vocoderGain, 0.0f, 8.0f) * 0.4f;
	const float flatten = ClampFloat(preset.vocoderFlatten, 0.0f, 1.0f);
	const float smooth = ClampFloat(preset.vocoderSmooth, 0.0f, 1.0f) * 0.5f;
	const float longTermFollow = 1.0f - std::exp(-1.0f / (1.2f * static_cast<float>(sampleRate)));
	float shaped[VOCODER_MAX_BANDS];
	float corrections[VOCODER_MAX_BANDS];
	std::fill(corrections, corrections + VOCODER_MAX_BANDS, 1.0f);

	for (int i = 0; i < samples; ++i) {
		const float voice = audio[i];
		float carrier = 0.0f;
		if (noiseWeight > 0.0f) carrier += NextRandom(state) * noiseWeight;
		if (buzzWeight > 0.0f) {
			carrier += (2.0f * state.vocoderBuzzPhase - 1.0f) * buzzWeight;
			state.vocoderBuzzPhase += buzzStep;
			if (state.vocoderBuzzPhase >= 1.0f) state.vocoderBuzzPhase -= 1.0f;
		}
		if (selfWeight > 0.0f) carrier += ClampFloat(voice * 0.01f, -1.0f, 1.0f) * selfWeight;

		float longTermMean = 0.0f;
		for (int b = 0; b < bands; ++b) {
			float level = voice;
			for (Biquad& filter : state.vocoderAnalysis[b]) level = filter.Process(level);
			level = std::fabs(level);
			float& envelope = state.vocoderVoiceEnvelope[b];
			envelope += (level > envelope ? attack : release) * (level - envelope);
			shaped[b] = envelope;

			if (flatten > 0.0f) {
				float& longTerm = state.vocoderLongTerm[b];
				longTerm += (longTerm <= 0.0f ? 1.0f : longTermFollow) * (envelope - longTerm);
				longTermMean += longTerm;
			}
		}

		if (flatten > 0.0f) {
			if ((i & (COMPRESSOR_CONTROL_SAMPLES - 1)) == 0) {
				longTermMean = longTermMean / static_cast<float>(bands) + 1e-3f;
				for (int b = 0; b < bands; ++b) {
					const float ratioToMean = longTermMean / (state.vocoderLongTerm[b] + longTermMean * 0.05f);
					corrections[b] = std::pow(ClampFloat(ratioToMean, 0.1f, 10.0f), flatten);
				}
			}
			for (int b = 0; b < bands; ++b) shaped[b] *= corrections[b];
		}

		if (smooth > 0.0f) {
			float previous = shaped[0];
			for (int b = 0; b < bands; ++b) {
				const float current = shaped[b];
				const float next = b + 1 < bands ? shaped[b + 1] : current;
				shaped[b] = current * (1.0f - smooth) + (previous + next) * 0.5f * smooth;
				previous = current;
			}
		}

		float wet = 0.0f;
		for (int b = 0; b < bands; ++b) {
			float band = carrier;
			for (Biquad& filter : state.vocoderSynthesis[b]) band = filter.Process(band);
			float& carrierEnvelope = state.vocoderCarrierEnvelope[b];
			carrierEnvelope += carrierFollow * (std::fabs(band) - carrierEnvelope);
			wet += band / (carrierEnvelope + 1e-4f) * shaped[b];
		}

		audio[i] = voice * (1.0f - mix) + wet * gain * mix;
	}
}

inline void ApplyWobble(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.wobbleHz <= 0.0f || preset.wobbleMs <= 0.0f) return;

	const float pi = 3.14159265358979323846f;
	const float depth = ClampFloat(preset.wobbleMs, 0.0f, 20.0f) * static_cast<float>(sampleRate) / 1000.0f;
	const float base = depth + 2.0f;
	const size_t size = static_cast<size_t>(base + depth) + 4;
	if (state.wobbleBuffer.size() != size) {
		state.wobbleBuffer.assign(size, 0.0f);
		state.wobbleWrite = 0;
	}

	const float step = 2.0f * pi * ClampFloat(preset.wobbleHz, 0.0f, 40.0f) / static_cast<float>(sampleRate);
	Phasor lfo(state.wobblePhase, step);
	for (int i = 0; i < samples; ++i) {
		state.wobbleBuffer[state.wobbleWrite] = audio[i];
		audio[i] = ReadFractional(state.wobbleBuffer, state.wobbleWrite, base + depth * lfo.Next());
		state.wobbleWrite = (state.wobbleWrite + 1) % size;
	}
	state.wobblePhase = std::fmod(state.wobblePhase + step * static_cast<float>(samples), 2.0f * pi);
}

/// A short feedback comb: the cavity of a mask, visor or tube in front of the mouth. A delay under about two milliseconds puts its first resonance in the speech band, which is what makes a respirator honk.
inline void ApplyTube(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.tubeMs <= 0.0f || preset.tubeMix <= 0.0f) return;

	const size_t length = static_cast<size_t>(std::max(1.0f,
		ClampFloat(preset.tubeMs, 0.05f, 30.0f) * static_cast<float>(sampleRate) / 1000.0f));
	if (state.tubeBuffer.size() != length) {
		state.tubeBuffer.assign(length, 0.0f);
		state.tubePosition = 0;
	}

	const float feedback = ClampFloat(preset.tubeFeedback, 0.0f, 0.95f);
	const float mix = ClampFloat(preset.tubeMix, 0.0f, 1.0f);
	const float normalise = 1.0f - feedback * 0.5f;
	for (int i = 0; i < samples; ++i) {
		const float dry = audio[i];
		const float wet = dry + state.tubeBuffer[state.tubePosition] * feedback;
		state.tubeBuffer[state.tubePosition] = wet;
		state.tubePosition = (state.tubePosition + 1) % length;
		audio[i] = dry * (1.0f - mix) + wet * normalise * mix;
	}
}

inline void ApplyCompressor(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.compressRatio <= 1.001f && std::fabs(preset.compressMakeupDb) < 0.01f) return;

	const float attack = 1.0f - std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate)));
	const float release = 1.0f - std::exp(-1.0f / (0.12f * static_cast<float>(sampleRate)));
	const float threshold = ClampFloat(preset.compressThresholdDb, -60.0f, 0.0f);
	const float slope = 1.0f / ClampFloat(preset.compressRatio, 1.0f, 40.0f) - 1.0f;
	const float makeup = std::pow(10.0f, ClampFloat(preset.compressMakeupDb, -24.0f, 36.0f) / 20.0f);
	const float thresholdLinear = std::pow(10.0f, threshold / 20.0f);
	for (int i = 0; i < samples; ++i) {
		const float level = std::fabs(audio[i]) / 32768.0f;
		state.compressorEnvelope += (level > state.compressorEnvelope ? attack : release) * (level - state.compressorEnvelope);
		if ((i & (COMPRESSOR_CONTROL_SAMPLES - 1)) == 0) {
			float target = makeup;
			if (state.compressorEnvelope > thresholdLinear) target *= std::pow(state.compressorEnvelope / thresholdLinear, slope);
			state.compressorGainStep = (target - state.compressorGain) / static_cast<float>(COMPRESSOR_CONTROL_SAMPLES);
		}
		state.compressorGain += state.compressorGainStep;
		audio[i] *= state.compressorGain;
	}
}

/// Simulates fading at the edge of range: at random moments the voice sinks for 30 to 120 milliseconds while the channel noise stays, the way a weak FM carrier drops under the static rather than going silent.
inline float ApplyDropout(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.dropout <= 0.0f) return 0.0f;

	const float chance = ClampFloat(preset.dropout, 0.0f, 1.0f) * 4.0f / static_cast<float>(sampleRate);
	const float smoothing = 1.0f - std::exp(-1.0f / (0.004f * static_cast<float>(sampleRate)));
	float deepest = 0.0f;
	for (int i = 0; i < samples; ++i) {
		if (state.dropoutRemaining > 0) {
			--state.dropoutRemaining;
		} else if (NextUnit(state) < chance) {
			state.dropoutRemaining = static_cast<int>((0.03f + NextUnit(state) * 0.09f) * static_cast<float>(sampleRate));
		}
		const float target = state.dropoutRemaining > 0 ? 0.06f : 1.0f;
		state.dropoutGain += (target - state.dropoutGain) * smoothing;
		audio[i] *= state.dropoutGain;
		deepest = std::max(deepest, 1.0f - state.dropoutGain);
	}
	return deepest;
}

/// Adds the noise sources a transmission picks up before the receiver's filters shape them: steady hiss (louder while the voice drops out), crackle impulses, and on the first block of a transmission the squelch burst and key-up chirp.
inline void ApplyChannelNoise(float* audio, int samples, const Preset& preset, int sampleRate, float fade, RuntimeState& state) {
	const float hiss = ClampFloat(preset.hiss, 0.0f, 0.25f) * 32767.0f * (1.0f + fade * 2.0f);
	const float crackle = ClampFloat(preset.crackle, 0.0f, 1.0f);
	const float crackleChance = crackle * 12.0f / static_cast<float>(sampleRate);
	const float crackleDecay = std::exp(-1.0f / (0.0006f * static_cast<float>(sampleRate)));
	const float pi = 3.14159265358979323846f;

	if (state.transmissionStart) {
		state.transmissionStart = false;
		if (preset.squelchMs > 0.0f && preset.squelchLevel > 0.0f) {
			state.squelchTotal = static_cast<int>(ClampFloat(preset.squelchMs, 0.0f, 1000.0f) * static_cast<float>(sampleRate) / 1000.0f);
			state.squelchRemaining = state.squelchTotal;
			state.crackleLevel = 0.9f;
		}
		if (preset.chirpHz > 0.0f && preset.chirpMs > 0.0f && preset.chirpLevel > 0.0f) {
			state.chirpPosition = 0;
			state.chirpPhase = 0.0f;
		}
	}

	if (hiss <= 0.0f && crackle <= 0.0f && state.squelchRemaining <= 0 && state.chirpPosition < 0) return;

	const int chirpLength = static_cast<int>(ClampFloat(preset.chirpMs, 0.0f, 500.0f) * static_cast<float>(sampleRate) / 1000.0f);
	const int chirpCount = std::max(1, std::min(4, preset.chirpCount));
	const int chirpFade = std::max(1, sampleRate / 333);
	const float chirpStep = 2.0f * pi * ClampFloat(preset.chirpHz, 0.0f, 8000.0f) / static_cast<float>(sampleRate);
	const float squelchLevel = ClampFloat(preset.squelchLevel, 0.0f, 1.0f) * 32767.0f;
	const float chirpLevel = ClampFloat(preset.chirpLevel, 0.0f, 1.0f) * 32767.0f;

	for (int i = 0; i < samples; ++i) {
		float added = NextRandom(state) * hiss;

		if (crackle > 0.0f && NextUnit(state) < crackleChance) state.crackleLevel = 0.3f + NextUnit(state) * 0.6f;
		if (state.crackleLevel > 0.001f) {
			added += NextRandom(state) * state.crackleLevel * crackle * 32767.0f;
			state.crackleLevel *= crackleDecay;
		}

		if (state.squelchRemaining > 0) {
			const float progress = 1.0f - static_cast<float>(state.squelchRemaining) / static_cast<float>(std::max(1, state.squelchTotal));
			const float envelope = progress < 0.6f ? 1.0f : 1.0f - (progress - 0.6f) / 0.4f;
			audio[i] *= 1.0f - envelope * 0.5f;
			added += NextRandom(state) * squelchLevel * envelope;
			--state.squelchRemaining;
		}

		if (state.chirpPosition >= 0) {
			const int period = chirpLength * 2;
			const int beep = state.chirpPosition / std::max(1, period);
			const int within = state.chirpPosition % std::max(1, period);
			if (beep >= chirpCount || chirpLength <= 0) {
				state.chirpPosition = -1;
			} else {
				if (within < chirpLength) {
					const float envelope = std::min(1.0f, std::min(static_cast<float>(within), static_cast<float>(chirpLength - within)) / static_cast<float>(chirpFade));
					const float tone = beep % 2 == 0 ? 1.0f : 0.75f;
					added += std::sin(state.chirpPhase * tone) * chirpLevel * envelope;
				}
				state.chirpPhase += chirpStep;
				if (state.chirpPhase >= 8.0f * pi) state.chirpPhase -= 8.0f * pi;
				++state.chirpPosition;
			}
		}

		audio[i] += added;
	}
}

inline void ApplyBand(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	const int order = std::max(1, std::min(2, preset.bandOrder));
	const bool lowCut = preset.bandLowHz > 0.0f;
	const bool highCut = preset.bandHighHz > 0.0f;
	bool anyPeak = false;
	for (int p = 0; p < PEAK_COUNT; ++p) {
		if (preset.peakHz[p] > 0.0f && std::fabs(preset.peakDb[p]) > 0.01f) {
			SetPeaking(state.peaks[p], preset.peakHz[p], ClampFloat(preset.peakDb[p], -24.0f, 24.0f), preset.peakQ[p], sampleRate);
			anyPeak = true;
		}
	}
	if (!lowCut && !highCut && !anyPeak) return;

	const float butterworth = 0.70710678f;
	for (int stage = 0; stage < order; ++stage) {
		if (lowCut) SetHighpass(state.bandLow[stage], preset.bandLowHz, butterworth, sampleRate);
		if (highCut) SetLowpass(state.bandHigh[stage], preset.bandHighHz, butterworth, sampleRate);
	}

	for (int i = 0; i < samples; ++i) {
		float value = audio[i];
		for (int stage = 0; stage < order; ++stage) {
			if (lowCut) value = state.bandLow[stage].Process(value);
			if (highCut) value = state.bandHigh[stage].Process(value);
		}
		for (int p = 0; p < PEAK_COUNT; ++p) {
			if (preset.peakHz[p] > 0.0f && std::fabs(preset.peakDb[p]) > 0.01f) value = state.peaks[p].Process(value);
		}
		audio[i] = value;
	}
}

/// A small mono Schroeder reverb (four damped feedback combs into two allpasses, Freeverb tunings rescaled to the voice sample rate) for the rooms a PA, a helmet or the water put around a voice.
inline void ApplyReverb(float* audio, int samples, const Preset& preset, int sampleRate, RuntimeState& state) {
	if (preset.reverbMix <= 0.0f) return;

	static const int combTunings[4] = { 1116, 1188, 1277, 1356 };
	static const int allpassTunings[2] = { 556, 441 };
	const float scale = static_cast<float>(sampleRate) / 44100.0f;
	for (int c = 0; c < 4; ++c) {
		const size_t length = static_cast<size_t>(std::max(1.0f, combTunings[c] * scale));
		if (state.reverbCombs[c].buffer.size() != length) {
			state.reverbCombs[c].buffer.assign(length, 0.0f);
			state.reverbCombs[c].position = 0;
			state.reverbCombs[c].store = 0.0f;
		}
	}
	for (int a = 0; a < 2; ++a) {
		const size_t length = static_cast<size_t>(std::max(1.0f, allpassTunings[a] * scale));
		if (state.reverbAllpasses[a].buffer.size() != length) {
			state.reverbAllpasses[a].buffer.assign(length, 0.0f);
			state.reverbAllpasses[a].position = 0;
		}
	}

	const float feedback = 0.7f + ClampFloat(preset.reverbSize, 0.0f, 1.0f) * 0.28f;
	const float damp = ClampFloat(preset.reverbDamp, 0.0f, 1.0f) * 0.4f;
	const float mix = ClampFloat(preset.reverbMix, 0.0f, 1.0f);
	for (int i = 0; i < samples; ++i) {
		const float input = audio[i] * 0.015f;
		float wet = 0.0f;
		for (ReverbComb& comb : state.reverbCombs) {
			const float output = comb.buffer[comb.position];
			comb.store = output * (1.0f - damp) + comb.store * damp;
			comb.buffer[comb.position] = input + comb.store * feedback;
			comb.position = (comb.position + 1) % comb.buffer.size();
			wet += output;
		}
		for (ReverbAllpass& allpass : state.reverbAllpasses) {
			const float buffered = allpass.buffer[allpass.position];
			allpass.buffer[allpass.position] = wet + buffered * 0.5f;
			allpass.position = (allpass.position + 1) % allpass.buffer.size();
			wet = buffered - wet;
		}
		audio[i] = audio[i] * (1.0f - mix * 0.5f) + wet * 20.0f * mix;
	}
}

inline void ApplyLimiter(float* audio, int samples) {
	const float knee = 0.8f * 32767.0f;
	const float headroom = 0.2f * 32767.0f;
	for (int i = 0; i < samples; ++i) {
		const float magnitude = std::fabs(audio[i]);
		if (magnitude <= knee) continue;
		const float limited = knee + headroom * std::tanh((magnitude - knee) / headroom);
		audio[i] = audio[i] < 0.0f ? -limited : limited;
	}
}

/// Runs one preset over a block of 16-bit samples. The original stages (pitch, one-pole filters, ring, downsample, drive, quantise, echo, white noise and gain) keep their order and arithmetic so existing presets sound as they did; every newer stage defaults off and slots in where it sits physically: the cavity after the mouth, the compressor and channel noise before the receiver's band filter, the room after the device.
inline void ApplyPreset(const Preset& preset, int16_t* audio, int samples, int sampleRate, RuntimeState& state) {
	if (audio == nullptr || samples <= 0 || preset.id == EFF_NONE) return;
	const float pi = 3.14159265358979323846f;

	static thread_local std::vector<float> buffer;
	if (buffer.size() < static_cast<size_t>(samples)) buffer.resize(static_cast<size_t>(samples));
	float* x = buffer.data();
	for (int i = 0; i < samples; ++i) x[i] = static_cast<float>(audio[i]);

	ApplyPitch(x, samples, ClampFloat(preset.pitch, 0.5f, 2.0f), sampleRate, state);
	ApplyVocoder(x, samples, preset, sampleRate, state);
	ApplyWobble(x, samples, preset, sampleRate, state);

	if (preset.highpassHz > 0.0f) {
		const float cutoff = ClampFloat(preset.highpassHz, 10.0f, sampleRate * 0.45f);
		const float rc = 1.0f / (2.0f * pi * cutoff);
		const float dt = 1.0f / static_cast<float>(sampleRate);
		const float alpha = rc / (rc + dt);
		for (int i = 0; i < samples; ++i) {
			const float input = x[i];
			state.highpassOutput = alpha * (state.highpassOutput + input - state.highpassInput);
			state.highpassInput = input;
			x[i] = ClampSample(state.highpassOutput);
		}
	}

	if (preset.lowpassHz > 0.0f) {
		const float cutoff = ClampFloat(preset.lowpassHz, 20.0f, sampleRate * 0.48f);
		const float rc = 1.0f / (2.0f * pi * cutoff);
		const float dt = 1.0f / static_cast<float>(sampleRate);
		const float alpha = dt / (rc + dt);
		for (int i = 0; i < samples; ++i) {
			state.lowpassOutput += alpha * (x[i] - state.lowpassOutput);
			x[i] = ClampSample(state.lowpassOutput);
		}
	}

	if (preset.ringFrequency > 0.0f && preset.ringMix > 0.0f) {
		const float mix = ClampFloat(preset.ringMix, 0.0f, 1.0f);
		const float step = 2.0f * pi * preset.ringFrequency / static_cast<float>(sampleRate);
		Phasor carrier(state.ringPhase, step);
		for (int i = 0; i < samples; ++i) {
			const float dry = x[i];
			x[i] = ClampSample(dry * (1.0f - mix) + dry * carrier.Next() * mix);
		}
		state.ringPhase = std::fmod(state.ringPhase + step * static_cast<float>(samples), 2.0f * pi);
	}

	ApplyTube(x, samples, preset, sampleRate, state);

	const int downsample = std::max(1, std::min(32, preset.downsample));
	if (downsample > 1) {
		for (int i = 0; i < samples; ++i) {
			if (state.holdRemaining <= 0) {
				state.heldSample = static_cast<int>(ClampSample(x[i]));
				state.holdRemaining = downsample;
			}
			x[i] = static_cast<float>(state.heldSample);
			--state.holdRemaining;
		}
	}

	if (preset.drive > 1.001f || preset.clip < 0.999f) {
		const float drive = ClampFloat(preset.drive, 0.05f, 16.0f);
		const float clip = ClampFloat(preset.clip, 0.05f, 1.0f);
		for (int i = 0; i < samples; ++i) {
			float normalized = ClampSample(x[i]) / 32768.0f;
			normalized *= drive;
			if (preset.softClip) normalized = std::tanh(normalized);
			normalized = ClampFloat(normalized, -clip, clip);
			x[i] = normalized * 32767.0f;
		}
	}

	if (preset.quantize > 1) {
		const int step = std::min(32767, preset.quantize);
		for (int i = 0; i < samples; ++i) {
			const int value = static_cast<int>(ClampSample(x[i]));
			x[i] = static_cast<float>((value / step) * step);
		}
	}

	ApplyCompressor(x, samples, preset, sampleRate, state);
	const float fade = ApplyDropout(x, samples, preset, sampleRate, state);
	ApplyChannelNoise(x, samples, preset, sampleRate, fade, state);
	ApplyBand(x, samples, preset, sampleRate, state);

	if (preset.echoDelayMs > 0.0f && preset.echoMix > 0.0f) {
		const int wantedDelay = std::max(1, std::min(sampleRate / 2,
			static_cast<int>(preset.echoDelayMs * static_cast<float>(sampleRate) / 1000.0f)));
		if (state.delay.size() != static_cast<size_t>(wantedDelay)) {
			state.delay.assign(static_cast<size_t>(wantedDelay), 0.0f);
			state.delayPosition = 0;
		}
		const float mix = ClampFloat(preset.echoMix, 0.0f, 0.95f);
		for (int i = 0; i < samples; ++i) {
			const float delayed = state.delay[state.delayPosition];
			const float dry = ClampSample(x[i]);
			x[i] = ClampSample(dry + delayed * mix);
			state.delay[state.delayPosition] = dry;
			state.delayPosition = (state.delayPosition + 1) % state.delay.size();
		}
	}

	ApplyReverb(x, samples, preset, sampleRate, state);

	const float noiseAmount = ClampFloat(preset.noise, 0.0f, 0.25f);
	const float gain = ClampFloat(preset.gain, 0.0f, 8.0f);
	if (noiseAmount > 0.0f || std::fabs(gain - 1.0f) > 0.001f) {
		for (int i = 0; i < samples; ++i) {
			x[i] = x[i] * gain + NextRandom(state) * noiseAmount * 32767.0f;
		}
	}

	if (preset.limit) ApplyLimiter(x, samples);

	for (int i = 0; i < samples; ++i) audio[i] = Clamp16(x[i]);
}

/// Runs a resolved chain (base preset first, then each layer in signal order) over one block. `states` holds one runtime per stage and is grown to fit; a stage's state is never shared with another stage, so a radio's squelch or a mask's cavity keeps its own memory.
inline void ApplyChain(const std::vector<Preset>& chain, int16_t* audio, int samples, int sampleRate,
	std::vector<RuntimeState>& states) {
	if (states.size() < chain.size()) states.resize(chain.size());
	for (size_t i = 0; i < chain.size(); ++i) ApplyPreset(chain[i], audio, samples, sampleRate, states[i]);
}

} // namespace AudioEffects
