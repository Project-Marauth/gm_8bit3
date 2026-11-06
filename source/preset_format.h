#pragma once

#include "audio_effects.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

/// The .gvox text format on its own, with no file access: shared by the server module's preset registry and the WebAssembly build of the voice lab, so both read a preset exactly the same way.
namespace PresetFormat {

inline std::string Trim(const std::string& input) {
	size_t begin = 0;
	while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) ++begin;
	size_t end = input.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) --end;
	return input.substr(begin, end - begin);
}

inline std::string Lower(std::string input) {
	std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return input;
}

inline bool EndsWithPresetExtension(const std::string& fileName) {
	const std::string lower = Lower(fileName);
	return lower.size() >= 5 && lower.substr(lower.size() - 5) == ".gvox";
}

inline bool ParseFloat(const std::string& text, float& value) {
	char* end = nullptr;
	errno = 0;
	const float parsed = std::strtof(text.c_str(), &end);
	if (errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
	value = parsed;
	return true;
}

inline bool ParseInt(const std::string& text, int& value) {
	char* end = nullptr;
	errno = 0;
	const long parsed = std::strtol(text.c_str(), &end, 10);
	if (errno != 0 || end == text.c_str() || *end != '\0' ||
		parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
	value = static_cast<int>(parsed);
	return true;
}

inline bool ParseBool(const std::string& text, bool& value) {
	const std::string lower = Lower(Trim(text));
	if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") { value = true; return true; }
	if (lower == "0" || lower == "false" || lower == "no" || lower == "off") { value = false; return true; }
	return false;
}

/// Parses `peak1_hz` / `peak1_db` / `peak1_q` through `peak3_*`. Returns whether the key was a peak key at all; `ok` reports whether its value parsed.
inline bool ParsePeakKey(const std::string& key, const std::string& value, AudioEffects::Preset& preset, bool& ok) {
	if (key.size() < 7 || key.compare(0, 4, "peak") != 0 || key[5] != '_') return false;
	const int index = key[4] - '1';
	if (index < 0 || index >= AudioEffects::PEAK_COUNT) return false;

	const std::string field = key.substr(6);
	if (field == "hz") ok = ParseFloat(value, preset.peakHz[index]);
	else if (field == "db") ok = ParseFloat(value, preset.peakDb[index]);
	else if (field == "q") ok = ParseFloat(value, preset.peakQ[index]);
	else return false;
	return true;
}

/// Parses .gvox text into a preset, clamping every field to its valid range. Returns false with a message for malformed lines, a missing version line or bad name, key or description lengths.
inline bool Parse(const std::string& content, const std::string& fileName,
	AudioEffects::Preset& preset, std::string& error) {
	if (content.empty() || content.size() > 65536) {
		error = "file is empty or larger than 64 KiB";
		return false;
	}
	if (!EndsWithPresetExtension(fileName) || fileName.size() <= 5 || fileName.find('/') != std::string::npos
		|| fileName.find('\\') != std::string::npos) {
		error = "invalid preset filename";
		return false;
	}
	std::istringstream input(content);

	preset = AudioEffects::Preset();
	preset.custom = true;
	preset.sourceFile = fileName;
	preset.key = Lower(fileName.substr(0, fileName.size() - 5));
	preset.name = preset.key;

	std::string line;
	int lineNumber = 0;
	bool hasVersion = false;
	while (std::getline(input, line)) {
		++lineNumber;
		if (lineNumber == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF
			&& static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
			line.erase(0, 3);
		}
		line = Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		const size_t separator = line.find('=');
		if (separator == std::string::npos) {
			error = "line " + std::to_string(lineNumber) + ": expected key=value";
			return false;
		}

		const std::string key = Lower(Trim(line.substr(0, separator)));
		const std::string value = Trim(line.substr(separator + 1));
		bool ok = true;
		if (key == "version") {
			int version = 0; ok = ParseInt(value, version) && version == 1; hasVersion = ok;
		} else if (key == "name") preset.name = value;
		else if (key == "description") preset.description = value;
		else if (key == "key") preset.key = Lower(value);
		else if (key == "gain") ok = ParseFloat(value, preset.gain);
		else if (key == "highpass" || key == "highpass_hz") ok = ParseFloat(value, preset.highpassHz);
		else if (key == "lowpass" || key == "lowpass_hz") ok = ParseFloat(value, preset.lowpassHz);
		else if (key == "drive") ok = ParseFloat(value, preset.drive);
		else if (key == "clip") ok = ParseFloat(value, preset.clip);
		else if (key == "pitch") ok = ParseFloat(value, preset.pitch);
		else if (key == "ring_frequency" || key == "ring_frequency_hz") ok = ParseFloat(value, preset.ringFrequency);
		else if (key == "ring_mix") ok = ParseFloat(value, preset.ringMix);
		else if (key == "echo_delay" || key == "echo_delay_ms") ok = ParseFloat(value, preset.echoDelayMs);
		else if (key == "echo_mix") ok = ParseFloat(value, preset.echoMix);
		else if (key == "noise") ok = ParseFloat(value, preset.noise);
		else if (key == "quantize") ok = ParseInt(value, preset.quantize);
		else if (key == "downsample") ok = ParseInt(value, preset.downsample);
		else if (key == "soft_clip") ok = ParseBool(value, preset.softClip);
		else if (key == "band_low_hz") ok = ParseFloat(value, preset.bandLowHz);
		else if (key == "band_high_hz") ok = ParseFloat(value, preset.bandHighHz);
		else if (key == "band_order") ok = ParseInt(value, preset.bandOrder);
		else if (ParsePeakKey(key, value, preset, ok)) {}
		else if (key == "tube_ms") ok = ParseFloat(value, preset.tubeMs);
		else if (key == "tube_feedback") ok = ParseFloat(value, preset.tubeFeedback);
		else if (key == "tube_mix") ok = ParseFloat(value, preset.tubeMix);
		else if (key == "compress_threshold_db") ok = ParseFloat(value, preset.compressThresholdDb);
		else if (key == "compress_ratio") ok = ParseFloat(value, preset.compressRatio);
		else if (key == "compress_makeup_db") ok = ParseFloat(value, preset.compressMakeupDb);
		else if (key == "hiss") ok = ParseFloat(value, preset.hiss);
		else if (key == "crackle") ok = ParseFloat(value, preset.crackle);
		else if (key == "dropout") ok = ParseFloat(value, preset.dropout);
		else if (key == "squelch_ms") ok = ParseFloat(value, preset.squelchMs);
		else if (key == "squelch_level") ok = ParseFloat(value, preset.squelchLevel);
		else if (key == "chirp_hz") ok = ParseFloat(value, preset.chirpHz);
		else if (key == "chirp_ms") ok = ParseFloat(value, preset.chirpMs);
		else if (key == "chirp_level") ok = ParseFloat(value, preset.chirpLevel);
		else if (key == "chirp_count") ok = ParseInt(value, preset.chirpCount);
		else if (key == "reverb_mix") ok = ParseFloat(value, preset.reverbMix);
		else if (key == "reverb_size") ok = ParseFloat(value, preset.reverbSize);
		else if (key == "reverb_damp") ok = ParseFloat(value, preset.reverbDamp);
		else if (key == "wobble_hz") ok = ParseFloat(value, preset.wobbleHz);
		else if (key == "wobble_ms") ok = ParseFloat(value, preset.wobbleMs);
		else if (key == "limit") ok = ParseBool(value, preset.limit);
		else if (key == "vocoder_mix") ok = ParseFloat(value, preset.vocoderMix);
		else if (key == "vocoder_bands") ok = ParseInt(value, preset.vocoderBands);
		else if (key == "vocoder_low_hz") ok = ParseFloat(value, preset.vocoderLowHz);
		else if (key == "vocoder_high_hz") ok = ParseFloat(value, preset.vocoderHighHz);
		else if (key == "vocoder_width") ok = ParseFloat(value, preset.vocoderWidth);
		else if (key == "vocoder_attack_ms") ok = ParseFloat(value, preset.vocoderAttackMs);
		else if (key == "vocoder_release_ms") ok = ParseFloat(value, preset.vocoderReleaseMs);
		else if (key == "vocoder_noise") ok = ParseFloat(value, preset.vocoderNoise);
		else if (key == "vocoder_buzz") ok = ParseFloat(value, preset.vocoderBuzz);
		else if (key == "vocoder_self") ok = ParseFloat(value, preset.vocoderSelf);
		else if (key == "vocoder_buzz_hz") ok = ParseFloat(value, preset.vocoderBuzzHz);
		else if (key == "vocoder_gain") ok = ParseFloat(value, preset.vocoderGain);
		else if (key == "vocoder_formant") ok = ParseFloat(value, preset.vocoderFormant);
		else if (key == "vocoder_flatten") ok = ParseFloat(value, preset.vocoderFlatten);
		else if (key == "vocoder_smooth") ok = ParseFloat(value, preset.vocoderSmooth);
		else continue;

		if (!ok) {
			error = "line " + std::to_string(lineNumber) + ": invalid value for " + key;
			return false;
		}
	}

	if (!hasVersion) {
		error = "missing version=1";
		return false;
	}
	if (preset.name.empty() || preset.name.size() > 96 || preset.key.empty() || preset.key.size() > 64 ||
		preset.description.size() > 192) {
		error = "invalid name/key/description length";
		return false;
	}

	preset.gain = AudioEffects::ClampFloat(preset.gain, 0.0f, 8.0f);
	preset.highpassHz = AudioEffects::ClampFloat(preset.highpassHz, 0.0f, 11000.0f);
	preset.lowpassHz = AudioEffects::ClampFloat(preset.lowpassHz, 0.0f, 11000.0f);
	preset.drive = AudioEffects::ClampFloat(preset.drive, 0.05f, 16.0f);
	preset.clip = AudioEffects::ClampFloat(preset.clip, 0.05f, 1.0f);
	preset.pitch = AudioEffects::ClampFloat(preset.pitch, 0.5f, 2.0f);
	preset.ringFrequency = AudioEffects::ClampFloat(preset.ringFrequency, 0.0f, 8000.0f);
	preset.ringMix = AudioEffects::ClampFloat(preset.ringMix, 0.0f, 1.0f);
	preset.echoDelayMs = AudioEffects::ClampFloat(preset.echoDelayMs, 0.0f, 500.0f);
	preset.echoMix = AudioEffects::ClampFloat(preset.echoMix, 0.0f, 0.95f);
	preset.noise = AudioEffects::ClampFloat(preset.noise, 0.0f, 0.25f);
	preset.quantize = std::max(0, std::min(32767, preset.quantize));
	preset.downsample = std::max(1, std::min(32, preset.downsample));
	preset.bandLowHz = AudioEffects::ClampFloat(preset.bandLowHz, 0.0f, 11000.0f);
	preset.bandHighHz = AudioEffects::ClampFloat(preset.bandHighHz, 0.0f, 11000.0f);
	preset.bandOrder = std::max(1, std::min(2, preset.bandOrder));
	for (int p = 0; p < AudioEffects::PEAK_COUNT; ++p) {
		preset.peakHz[p] = AudioEffects::ClampFloat(preset.peakHz[p], 0.0f, 11000.0f);
		preset.peakDb[p] = AudioEffects::ClampFloat(preset.peakDb[p], -24.0f, 24.0f);
		preset.peakQ[p] = AudioEffects::ClampFloat(preset.peakQ[p], 0.1f, 24.0f);
	}
	preset.tubeMs = AudioEffects::ClampFloat(preset.tubeMs, 0.0f, 30.0f);
	preset.tubeFeedback = AudioEffects::ClampFloat(preset.tubeFeedback, 0.0f, 0.95f);
	preset.tubeMix = AudioEffects::ClampFloat(preset.tubeMix, 0.0f, 1.0f);
	preset.compressThresholdDb = AudioEffects::ClampFloat(preset.compressThresholdDb, -60.0f, 0.0f);
	preset.compressRatio = AudioEffects::ClampFloat(preset.compressRatio, 1.0f, 40.0f);
	preset.compressMakeupDb = AudioEffects::ClampFloat(preset.compressMakeupDb, -24.0f, 36.0f);
	preset.hiss = AudioEffects::ClampFloat(preset.hiss, 0.0f, 0.25f);
	preset.crackle = AudioEffects::ClampFloat(preset.crackle, 0.0f, 1.0f);
	preset.dropout = AudioEffects::ClampFloat(preset.dropout, 0.0f, 1.0f);
	preset.squelchMs = AudioEffects::ClampFloat(preset.squelchMs, 0.0f, 1000.0f);
	preset.squelchLevel = AudioEffects::ClampFloat(preset.squelchLevel, 0.0f, 1.0f);
	preset.chirpHz = AudioEffects::ClampFloat(preset.chirpHz, 0.0f, 8000.0f);
	preset.chirpMs = AudioEffects::ClampFloat(preset.chirpMs, 0.0f, 500.0f);
	preset.chirpLevel = AudioEffects::ClampFloat(preset.chirpLevel, 0.0f, 1.0f);
	preset.chirpCount = std::max(1, std::min(4, preset.chirpCount));
	preset.reverbMix = AudioEffects::ClampFloat(preset.reverbMix, 0.0f, 1.0f);
	preset.reverbSize = AudioEffects::ClampFloat(preset.reverbSize, 0.0f, 1.0f);
	preset.reverbDamp = AudioEffects::ClampFloat(preset.reverbDamp, 0.0f, 1.0f);
	preset.wobbleHz = AudioEffects::ClampFloat(preset.wobbleHz, 0.0f, 40.0f);
	preset.wobbleMs = AudioEffects::ClampFloat(preset.wobbleMs, 0.0f, 20.0f);
	preset.vocoderMix = AudioEffects::ClampFloat(preset.vocoderMix, 0.0f, 1.0f);
	preset.vocoderBands = std::max(4, std::min(AudioEffects::VOCODER_MAX_BANDS, preset.vocoderBands));
	preset.vocoderLowHz = AudioEffects::ClampFloat(preset.vocoderLowHz, 40.0f, 9000.0f);
	preset.vocoderHighHz = AudioEffects::ClampFloat(preset.vocoderHighHz, 60.0f, 10800.0f);
	preset.vocoderWidth = AudioEffects::ClampFloat(preset.vocoderWidth, 0.25f, 4.0f);
	preset.vocoderAttackMs = AudioEffects::ClampFloat(preset.vocoderAttackMs, 0.1f, 50.0f);
	preset.vocoderReleaseMs = AudioEffects::ClampFloat(preset.vocoderReleaseMs, 1.0f, 200.0f);
	preset.vocoderNoise = AudioEffects::ClampFloat(preset.vocoderNoise, 0.0f, 1.0f);
	preset.vocoderBuzz = AudioEffects::ClampFloat(preset.vocoderBuzz, 0.0f, 1.0f);
	preset.vocoderSelf = AudioEffects::ClampFloat(preset.vocoderSelf, 0.0f, 1.0f);
	preset.vocoderBuzzHz = AudioEffects::ClampFloat(preset.vocoderBuzzHz, 20.0f, 1000.0f);
	preset.vocoderGain = AudioEffects::ClampFloat(preset.vocoderGain, 0.0f, 8.0f);
	preset.vocoderFormant = AudioEffects::ClampFloat(preset.vocoderFormant, 0.5f, 2.0f);
	preset.vocoderFlatten = AudioEffects::ClampFloat(preset.vocoderFlatten, 0.0f, 1.0f);
	preset.vocoderSmooth = AudioEffects::ClampFloat(preset.vocoderSmooth, 0.0f, 1.0f);
	return true;
}

} // namespace PresetFormat
