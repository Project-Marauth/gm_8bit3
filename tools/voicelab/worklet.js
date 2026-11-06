/**
 * Voice lab audio thread: runs the WebAssembly engine on every 128-sample render quantum at 24 kHz.
 *
 * Transmission starts mirror the server: with push-to-talk the line is silent until the talk key is held, and each
 * press starts a transmission; in open-mic mode a transmission starts whenever speech follows 350 ms of silence.
 */

const TRANSMISSION_GAP_SECONDS = 0.35;
const SPEECH_THRESHOLD = 0.01;

class VoiceLabProcessor extends AudioWorkletProcessor {
	constructor() {
		super();
		this.engine = null;
		this.lab = 0;
		this.pendingChain = null;
		this.bypass = false;
		this.pushToTalk = false;
		this.talking = false;
		this.silentSeconds = Infinity;
		this.levelPeak = 0;
		this.levelFrames = 0;
		this.port.onmessage = (event) => this.receive(event.data);
	}

	async receive(message) {
		if (message.type === "engine") {
			const module = await WebAssembly.compile(message.bytes);
			const imports = {};
			for (const entry of WebAssembly.Module.imports(module)) {
				(imports[entry.module] ??= {})[entry.name] = () => 0;
			}
			const instance = await WebAssembly.instantiate(module, imports);
			this.engine = instance.exports;
			this.engine._initialize?.();
			this.lab = this.engine.vl_create();
			if (this.pendingChain !== null) this.setChain(this.pendingChain);
		} else if (message.type === "chain") {
			if (this.engine) this.setChain(message.text);
			else this.pendingChain = message.text;
		} else if (message.type === "begin") {
			this.begin();
		} else if (message.type === "bypass") {
			this.bypass = message.value;
		} else if (message.type === "pushToTalk") {
			this.pushToTalk = message.value;
		} else if (message.type === "talk") {
			if (message.value && !this.talking) this.begin();
			this.talking = message.value;
		}
	}

	setChain(text) {
		const bytes = new TextEncoder().encode(text);
		const pointer = this.engine.vl_text_buffer(this.lab, bytes.length);
		new Uint8Array(this.engine.memory.buffer, pointer, bytes.length).set(bytes);
		const result = this.engine.vl_set_chain(this.lab, bytes.length);
		if (result < 0) {
			const length = this.engine.vl_message_length(this.lab);
			const text = new TextDecoder().decode(new Uint8Array(this.engine.memory.buffer, this.engine.vl_message(this.lab), length));
			this.port.postMessage({ type: "error", text });
		} else {
			this.port.postMessage({ type: "chain", stages: result });
		}
	}

	begin() {
		if (this.engine) this.engine.vl_begin(this.lab);
		this.port.postMessage({ type: "keyed" });
	}

	process(inputs, outputs) {
		const input = inputs[0]?.[0];
		const output = outputs[0][0];
		if (!output) return true;
		if (!input) {
			output.fill(0);
			return true;
		}

		if (this.pushToTalk && !this.talking) {
			output.fill(0);
			this.report(output);
			return true;
		}

		let peak = 0;
		for (let i = 0; i < input.length; i++) peak = Math.max(peak, Math.abs(input[i]));
		if (!this.pushToTalk) {
			if (peak > SPEECH_THRESHOLD) {
				if (this.silentSeconds > TRANSMISSION_GAP_SECONDS) this.begin();
				this.silentSeconds = 0;
			} else {
				this.silentSeconds += input.length / sampleRate;
			}
		}

		if (this.bypass || !this.engine) {
			output.set(input);
		} else {
			const pointer = this.engine.vl_audio_buffer(this.lab, input.length);
			new Float32Array(this.engine.memory.buffer, pointer, input.length).set(input);
			this.engine.vl_process(this.lab, input.length);
			output.set(new Float32Array(this.engine.memory.buffer, this.engine.vl_audio_buffer(this.lab, input.length), input.length));
		}

		this.report(output);
		return true;
	}

	report(output) {
		for (let i = 0; i < output.length; i++) this.levelPeak = Math.max(this.levelPeak, Math.abs(output[i]));
		this.levelFrames += output.length;
		if (this.levelFrames < sampleRate / 30) return;
		this.port.postMessage({ type: "level", peak: this.levelPeak });
		this.levelPeak = 0;
		this.levelFrames = 0;
	}
}

registerProcessor("voicelab", VoiceLabProcessor);
