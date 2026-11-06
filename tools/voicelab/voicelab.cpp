/// Voice lab renderer: runs the module's own effect engine (audio_effects.h and the .gvox parser) over a WAV file, so presets can be auditioned and tuned without building or loading the server DLL. Audio is processed in 20 ms packets at 24 kHz with a transmission start on the first packet, the same way the server hook feeds it; the Opus round trip the server adds is not simulated.
///
/// Usage:
///   voicelab list                               Prints every built-in preset as JSON, each with its full .gvox text.
///   voicelab render <in.wav> <out.wav> <stage>  Renders a chain. Each stage is "builtin:<id>" or a path to a .gvox file.

#include "audio_effects.h"
#include "voice_presets.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int SAMPLE_RATE = 24000;
constexpr int PACKET_SAMPLES = 480;

std::string Number(float value) {
	char text[32];
	std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
	return text;
}

/// Writes every field of a preset as .gvox text, so a built-in can be copied, edited and loaded back as a custom preset.
std::string ToGvox(const AudioEffects::Preset& p) {
	std::ostringstream out;
	out << "version=1\n";
	out << "name=" << p.name << "\n";
	out << "description=" << p.description << "\n";
	const std::pair<const char*, float> fields[] = {
		{ "gain", p.gain }, { "pitch", p.pitch },
		{ "vocoder_mix", p.vocoderMix }, { "vocoder_bands", static_cast<float>(p.vocoderBands) },
		{ "vocoder_low_hz", p.vocoderLowHz }, { "vocoder_high_hz", p.vocoderHighHz },
		{ "vocoder_width", p.vocoderWidth }, { "vocoder_attack_ms", p.vocoderAttackMs },
		{ "vocoder_release_ms", p.vocoderReleaseMs }, { "vocoder_noise", p.vocoderNoise },
		{ "vocoder_buzz", p.vocoderBuzz }, { "vocoder_self", p.vocoderSelf },
		{ "vocoder_buzz_hz", p.vocoderBuzzHz }, { "vocoder_gain", p.vocoderGain },
		{ "wobble_hz", p.wobbleHz }, { "wobble_ms", p.wobbleMs },
		{ "highpass_hz", p.highpassHz }, { "lowpass_hz", p.lowpassHz },
		{ "ring_frequency_hz", p.ringFrequency }, { "ring_mix", p.ringMix },
		{ "tube_ms", p.tubeMs }, { "tube_feedback", p.tubeFeedback }, { "tube_mix", p.tubeMix },
		{ "downsample", static_cast<float>(p.downsample) }, { "drive", p.drive }, { "clip", p.clip },
		{ "quantize", static_cast<float>(p.quantize) },
		{ "compress_threshold_db", p.compressThresholdDb }, { "compress_ratio", p.compressRatio },
		{ "compress_makeup_db", p.compressMakeupDb },
		{ "dropout", p.dropout }, { "hiss", p.hiss }, { "crackle", p.crackle },
		{ "squelch_ms", p.squelchMs }, { "squelch_level", p.squelchLevel },
		{ "chirp_hz", p.chirpHz }, { "chirp_ms", p.chirpMs }, { "chirp_level", p.chirpLevel },
		{ "chirp_count", static_cast<float>(p.chirpCount) },
		{ "band_low_hz", p.bandLowHz }, { "band_high_hz", p.bandHighHz },
		{ "band_order", static_cast<float>(p.bandOrder) },
		{ "peak1_hz", p.peakHz[0] }, { "peak1_db", p.peakDb[0] }, { "peak1_q", p.peakQ[0] },
		{ "peak2_hz", p.peakHz[1] }, { "peak2_db", p.peakDb[1] }, { "peak2_q", p.peakQ[1] },
		{ "peak3_hz", p.peakHz[2] }, { "peak3_db", p.peakDb[2] }, { "peak3_q", p.peakQ[2] },
		{ "echo_delay_ms", p.echoDelayMs }, { "echo_mix", p.echoMix },
		{ "reverb_mix", p.reverbMix }, { "reverb_size", p.reverbSize }, { "reverb_damp", p.reverbDamp },
		{ "noise", p.noise }
	};
	for (const auto& field : fields) out << field.first << "=" << Number(field.second) << "\n";
	out << "soft_clip=" << (p.softClip ? "true" : "false") << "\n";
	out << "limit=" << (p.limit ? "true" : "false") << "\n";
	return out.str();
}

std::string JsonString(const std::string& text) {
	std::string out = "\"";
	for (char c : text) {
		if (c == '"' || c == '\\') { out += '\\'; out += c; }
		else if (c == '\n') out += "\\n";
		else out += c;
	}
	return out + "\"";
}

int List() {
	int layerCount = 0;
	const AudioEffects::LayerInfo* layers = AudioEffects::Layers(layerCount);
	std::cout << "{\"presets\":[";
	for (int id = AudioEffects::EFF_NONE; id <= AudioEffects::EFF_BUILTIN_LAST; ++id) {
		const AudioEffects::Preset preset = AudioEffects::MakeBuiltin(id, 1.2f, 350, 2);
		if (id > 0) std::cout << ",";
		std::cout << "{\"id\":" << id << ",\"key\":" << JsonString(preset.key) << ",\"name\":" << JsonString(preset.name)
			<< ",\"description\":" << JsonString(preset.description) << ",\"gvox\":" << JsonString(ToGvox(preset)) << "}";
	}
	std::cout << "],\"layers\":[";
	for (int i = 0; i < layerCount; ++i) {
		if (i > 0) std::cout << ",";
		std::cout << "{\"key\":" << JsonString(layers[i].key) << ",\"name\":" << JsonString(layers[i].name)
			<< ",\"preset\":" << layers[i].presetId << "}";
	}
	std::cout << "]}\n";
	return 0;
}

bool ReadFile(const std::string& path, std::string& content) {
	std::ifstream input(path, std::ios::binary);
	if (!input) return false;
	content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	return true;
}

bool ReadWav(const std::string& path, std::vector<int16_t>& samples) {
	std::string data;
	if (!ReadFile(path, data) || data.size() < 12 || data.compare(0, 4, "RIFF") != 0) return false;

	uint16_t channels = 0, bits = 0;
	uint32_t rate = 0;
	size_t position = 12;
	while (position + 8 <= data.size()) {
		uint32_t size = 0;
		std::memcpy(&size, &data[position + 4], 4);
		const std::string id = data.substr(position, 4);
		if (id == "fmt " && size >= 16) {
			std::memcpy(&channels, &data[position + 10], 2);
			std::memcpy(&rate, &data[position + 12], 4);
			std::memcpy(&bits, &data[position + 22], 2);
		} else if (id == "data") {
			if (channels != 1 || bits != 16 || rate != SAMPLE_RATE) {
				std::cerr << "input must be 16-bit mono 24000 Hz, got " << bits << "-bit " << channels << " ch " << rate << " Hz\n";
				return false;
			}
			const size_t bytes = std::min<size_t>(size, data.size() - position - 8);
			samples.resize(bytes / 2);
			std::memcpy(samples.data(), &data[position + 8], samples.size() * 2);
			return true;
		}
		position += 8 + size + (size & 1);
	}
	return false;
}

bool WriteWav(const std::string& path, const std::vector<int16_t>& samples) {
	std::ofstream out(path, std::ios::binary);
	if (!out) return false;
	const uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
	const uint32_t riffSize = 36 + dataSize, fmtSize = 16, rate = SAMPLE_RATE, byteRate = SAMPLE_RATE * 2;
	const uint16_t format = 1, channels = 1, align = 2, bits = 16;
	out.write("RIFF", 4); out.write(reinterpret_cast<const char*>(&riffSize), 4); out.write("WAVEfmt ", 8);
	out.write(reinterpret_cast<const char*>(&fmtSize), 4); out.write(reinterpret_cast<const char*>(&format), 2);
	out.write(reinterpret_cast<const char*>(&channels), 2); out.write(reinterpret_cast<const char*>(&rate), 4);
	out.write(reinterpret_cast<const char*>(&byteRate), 4); out.write(reinterpret_cast<const char*>(&align), 2);
	out.write(reinterpret_cast<const char*>(&bits), 2); out.write("data", 4);
	out.write(reinterpret_cast<const char*>(&dataSize), 4);
	out.write(reinterpret_cast<const char*>(samples.data()), dataSize);
	return static_cast<bool>(out);
}

/// Resolves one stage argument: a built-in id, or a .gvox file run through the module's own parser so the lab accepts exactly what the server would.
bool LoadStage(const std::string& argument, AudioEffects::Preset& preset) {
	if (argument.compare(0, 8, "builtin:") == 0) {
		const int id = std::atoi(argument.c_str() + 8);
		if (id < AudioEffects::EFF_NONE || id > AudioEffects::EFF_BUILTIN_LAST) return false;
		preset = AudioEffects::MakeBuiltin(id, 1.2f, 350, 2);
		return true;
	}

	std::string content;
	if (!ReadFile(argument, content)) { std::cerr << "cannot read " << argument << "\n"; return false; }

	VoicePresetRegistry registry;
	registry.ReloadFromMemory({ { "stage.gvox", content } });
	for (const std::string& error : registry.Errors()) std::cerr << error << "\n";
	if (registry.CustomPresets().empty()) return false;
	preset = registry.CustomPresets().front();
	return true;
}

int Render(int argc, char** argv) {
	if (argc < 4) { std::cerr << "usage: voicelab render <in.wav> <out.wav> <stage>...\n"; return 2; }

	std::vector<int16_t> samples;
	if (!ReadWav(argv[2], samples)) { std::cerr << "cannot read input wav\n"; return 1; }

	std::vector<AudioEffects::Preset> chain;
	for (int i = 4; i < argc; ++i) {
		AudioEffects::Preset preset;
		if (!LoadStage(argv[i], preset)) { std::cerr << "invalid stage " << argv[i] << "\n"; return 1; }
		if (preset.id != AudioEffects::EFF_NONE) chain.push_back(preset);
	}

	std::vector<AudioEffects::RuntimeState> states(chain.size());
	for (AudioEffects::RuntimeState& state : states) AudioEffects::BeginTransmission(state);

	const size_t whole = samples.size() / PACKET_SAMPLES * PACKET_SAMPLES;
	for (size_t offset = 0; offset < whole; offset += PACKET_SAMPLES) {
		AudioEffects::ApplyChain(chain, samples.data() + offset, PACKET_SAMPLES, SAMPLE_RATE, states);
	}
	samples.resize(whole);

	if (!WriteWav(argv[3], samples)) { std::cerr << "cannot write output wav\n"; return 1; }
	return 0;
}

} // namespace

int main(int argc, char** argv) {
	if (argc >= 2 && std::strcmp(argv[1], "list") == 0) return List();
	if (argc >= 2 && std::strcmp(argv[1], "render") == 0) return Render(argc, argv);
	std::cerr << "usage: voicelab list | voicelab render <in.wav> <out.wav> <stage>...\n";
	return 2;
}
