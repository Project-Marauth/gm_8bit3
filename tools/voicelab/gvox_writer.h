#pragma once

#include "audio_effects.h"

#include <cstdio>
#include <sstream>
#include <string>
#include <utility>

/// Writes presets back out as .gvox text and the built-in catalogue as JSON, for the voice lab's command-line renderer and its WebAssembly engine alike.
namespace GvoxWriter {

inline std::string Number(float value) {
	char text[32];
	std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
	return text;
}

/// Writes every field of a preset as .gvox text, so a built-in can be copied, edited and loaded back as a custom preset.
inline std::string ToGvox(const AudioEffects::Preset& p) {
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
		{ "vocoder_formant", p.vocoderFormant }, { "vocoder_flatten", p.vocoderFlatten },
		{ "vocoder_smooth", p.vocoderSmooth },
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

inline std::string JsonString(const std::string& text) {
	std::string out = "\"";
	for (char c : text) {
		if (c == '"' || c == '\\') { out += '\\'; out += c; }
		else if (c == '\n') out += "\\n";
		else out += c;
	}
	return out + "\"";
}

/// Every built-in preset as JSON, each with its full .gvox text, plus the layer table: the catalogue the voice lab builds its menus from.
inline std::string Catalogue() {
	int layerCount = 0;
	const AudioEffects::LayerInfo* layers = AudioEffects::Layers(layerCount);
	std::ostringstream out;
	out << "{\"presets\":[";
	for (int id = AudioEffects::EFF_NONE; id <= AudioEffects::EFF_BUILTIN_LAST; ++id) {
		const AudioEffects::Preset preset = AudioEffects::MakeBuiltin(id, 1.2f, 350, 2);
		if (id > 0) out << ",";
		out << "{\"id\":" << id << ",\"key\":" << JsonString(preset.key) << ",\"name\":" << JsonString(preset.name)
			<< ",\"description\":" << JsonString(preset.description) << ",\"gvox\":" << JsonString(ToGvox(preset)) << "}";
	}
	out << "],\"layers\":[";
	for (int i = 0; i < layerCount; ++i) {
		if (i > 0) out << ",";
		out << "{\"key\":" << JsonString(layers[i].key) << ",\"name\":" << JsonString(layers[i].name)
			<< ",\"preset\":" << layers[i].presetId << "}";
	}
	out << "]}";
	return out.str();
}

} // namespace GvoxWriter
