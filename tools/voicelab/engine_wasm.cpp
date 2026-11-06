/// The voice lab's real-time engine: the module's own effect chain and preset parser compiled to WebAssembly, so the page can run them inside an AudioWorklet on live microphone input. Build with `python tools/voicelab/build_wasm.py`.
///
/// The page talks to it through a handful of C exports: it writes text or audio into buffers the engine hands out, then calls the matching function. All audio is mono float at 24 kHz, the rate the server processes voice at.

#include "audio_effects.h"
#include "gvox_writer.h"
#include "preset_format.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#define EXPORT(name) extern "C" __attribute__((export_name(name)))

namespace {

constexpr int SAMPLE_RATE = 24000;
constexpr const char* STAGE_SEPARATOR = "\n---\n";

struct Lab {
	std::vector<AudioEffects::Preset> chain;
	std::vector<AudioEffects::RuntimeState> states;
	std::vector<float> audio;
	std::vector<int16_t> pcm;
	std::string text;
	std::string message;
};

/// Splits the posted chain text into stages and parses each, so a bad stage is reported without disturbing the chain that is already playing.
bool ParseChain(const std::string& text, std::vector<AudioEffects::Preset>& chain, std::string& error) {
	chain.clear();
	size_t start = 0;
	int index = 0;
	while (start <= text.size()) {
		size_t end = text.find(STAGE_SEPARATOR, start);
		if (end == std::string::npos) end = text.size();
		const std::string stage = text.substr(start, end - start);
		start = end + std::string(STAGE_SEPARATOR).size();
		++index;
		if (PresetFormat::Trim(stage).empty()) {
			if (end == text.size()) break;
			continue;
		}

		AudioEffects::Preset preset;
		std::string stageError;
		if (!PresetFormat::Parse(stage, "stage.gvox", preset, stageError)) {
			error = "stage " + std::to_string(index) + ": " + stageError;
			return false;
		}
		preset.id = AudioEffects::CUSTOM_ID_MIN + index;
		chain.push_back(preset);
		if (end == text.size()) break;
	}
	return true;
}

} // namespace

/// The engine's per-thread scratch buffer registers a destructor with the C++ runtime, which this single-threaded WebAssembly build has no thread exit to run; accepting and ignoring the registration is correct here.
extern "C" int __cxa_thread_atexit(void (*)(void*), void*, void*) {
	return 0;
}

EXPORT("vl_create") Lab* Create() {
	return new Lab();
}

EXPORT("vl_destroy") void Destroy(Lab* lab) {
	delete lab;
}

/// Returns a buffer of at least `bytes` for the page to write chain text into before calling vl_set_chain.
EXPORT("vl_text_buffer") char* TextBuffer(Lab* lab, int bytes) {
	lab->text.assign(static_cast<size_t>(std::max(0, bytes)), '\0');
	return &lab->text[0];
}

/// Replaces the chain with the stages written into the text buffer (each a .gvox document, separated by a line of three dashes). Runtime state is kept when the stage count is unchanged, so dragging a slider never clicks or restarts a delay line. Returns the stage count, or -1 with the reason in vl_message.
EXPORT("vl_set_chain") int SetChain(Lab* lab, int bytes) {
	std::vector<AudioEffects::Preset> chain;
	std::string error;
	if (!ParseChain(lab->text.substr(0, static_cast<size_t>(std::max(0, bytes))), chain, error)) {
		lab->message = error;
		return -1;
	}
	if (lab->states.size() != chain.size()) lab->states.assign(chain.size(), AudioEffects::RuntimeState());
	lab->chain = chain;
	lab->message.clear();
	return static_cast<int>(chain.size());
}

/// Starts a new transmission on every stage: delay lines clear and squelch bursts and key-up chirps play, as when a player keys their microphone in game.
EXPORT("vl_begin") void Begin(Lab* lab) {
	for (AudioEffects::RuntimeState& state : lab->states) AudioEffects::BeginTransmission(state);
}

/// Returns a float buffer of `samples` for the page to fill with input and read output from.
EXPORT("vl_audio_buffer") float* AudioBuffer(Lab* lab, int samples) {
	lab->audio.resize(static_cast<size_t>(std::max(0, samples)));
	lab->pcm.resize(lab->audio.size());
	return lab->audio.data();
}

/// Runs the chain over the audio buffer in place. Input is converted to 16-bit first, exactly like the decoded voice packets the server works on.
EXPORT("vl_process") void Process(Lab* lab, int samples) {
	const size_t count = std::min(lab->audio.size(), static_cast<size_t>(std::max(0, samples)));
	for (size_t i = 0; i < count; ++i) lab->pcm[i] = AudioEffects::Clamp16(std::nearbyint(lab->audio[i] * 32767.0f));
	AudioEffects::ApplyChain(lab->chain, lab->pcm.data(), static_cast<int>(count), SAMPLE_RATE, lab->states);
	for (size_t i = 0; i < count; ++i) lab->audio[i] = static_cast<float>(lab->pcm[i]) / 32768.0f;
}

/// Writes the built-in catalogue as JSON into the message buffer and returns its length.
EXPORT("vl_catalogue") int Catalogue(Lab* lab) {
	lab->message = GvoxWriter::Catalogue();
	return static_cast<int>(lab->message.size());
}

EXPORT("vl_message") const char* Message(Lab* lab) {
	return lab->message.c_str();
}

EXPORT("vl_message_length") int MessageLength(Lab* lab) {
	return static_cast<int>(lab->message.size());
}
