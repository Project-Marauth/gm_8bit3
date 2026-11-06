/**
 * Voice lab main thread: chain editing, variants, library, sources and the live spectrogram. Audio runs in
 * worklet.js on the WebAssembly build of the module's own engine; a second engine instance here supplies the
 * built-in catalogue, validates presets and renders clips to WAV.
 */

const SAMPLE_RATE = 24000;
const SLIDER_STEPS = 1000;
const STORAGE_KEY = "eightbit.voicelab.v2";
const STAGE_SEPARATOR = "\n---\n";
const SLOTS = ["A", "B", "C", "D"];

/** Field layout in signal order. Each entry: key, label, min, max, step, scale, help. */
const SECTIONS = [
	{ title: "Voice", active: () => true, fields: [
		["pitch", "Pitch", 0.5, 2, 0.01, "lin", "Shifts the voice before everything else. Also moves its formants, so it changes the apparent size of the speaker."],
		["wobble_hz", "Wobble rate", 0, 40, 0.1, "lin", "Speed of a slow pitch waver (vibrato)."],
		["wobble_ms", "Wobble depth", 0, 20, 0.1, "lin", "How far the waver swings, in milliseconds of delay."],
	]},
	{ title: "Vocoder", active: (v) => v.vocoder_mix > 0, fields: [
		["vocoder_mix", "Mix", 0, 1, 0.01, "lin", "Vocoded voice against the dry voice. Any dry voice left in gives the speaker away."],
		["vocoder_bands", "Bands", 4, 32, 1, "lin", "More bands keep words clear; fewer sound more machine-like and hide more of who is talking."],
		["vocoder_low_hz", "Low edge", 40, 2000, 1, "log", "Bottom of the lowest band."],
		["vocoder_high_hz", "High edge", 600, 10000, 1, "log", "Top of the highest band. Consonants live above 3 kHz."],
		["vocoder_width", "Band width", 0.25, 4, 0.01, "lin", "Below 1 leaves gaps between bands (metallic, ringing); above 1 overlaps them (smeared)."],
		["vocoder_attack_ms", "Attack", 0.1, 50, 0.1, "lin", "How fast a band opens when the voice gets louder in it."],
		["vocoder_release_ms", "Release", 1, 200, 1, "lin", "How fast a band closes again. Short is crisp and choppy, long is smooth and slurred."],
		["vocoder_noise", "Carrier: noise", 0, 1, 0.01, "lin", "Whispered, unpitched carrier. Hides the speaker's pitch."],
		["vocoder_buzz", "Carrier: buzz", 0, 1, 0.01, "lin", "Monotone sawtooth carrier at the buzz pitch. Robotic, hides intonation."],
		["vocoder_self", "Carrier: own voice", 0, 1, 0.01, "lin", "The speaker's own voice, clipped. Keeps intonation and character, and with them identity."],
		["vocoder_buzz_hz", "Buzz pitch", 20, 500, 1, "log", "Pitch of the buzz carrier."],
		["vocoder_gain", "Vocoder level", 0, 4, 0.01, "lin", "Output level of the vocoder itself."],
		["vocoder_formant", "Formant shift", 0.5, 2, 0.01, "lin", "Moves every band. Above 1 sounds smaller, below 1 bigger. Independent of pitch."],
		["vocoder_flatten", "Flatten timbre", 0, 1, 0.01, "lin", "Levels each band against its own long-term average, removing the speaker's vocal colour."],
		["vocoder_smooth", "Smooth bands", 0, 1, 0.01, "lin", "Blurs neighbouring bands, discarding the fine detail that tells two voices apart."],
	]},
	{ title: "Modulation", active: (v) => v.ring_mix > 0, fields: [
		["ring_frequency_hz", "Ring frequency", 0, 2000, 1, "log", "Ring modulator: low values throb, high values sound metallic and alien."],
		["ring_mix", "Ring mix", 0, 1, 0.01, "lin", "How much ring modulation is blended in."],
	]},
	{ title: "Mask cavity", active: (v) => v.tube_mix > 0, fields: [
		["tube_ms", "Cavity size", 0, 30, 0.05, "lin", "Delay of the resonating space in front of the mouth. Under 2 ms honks like a respirator."],
		["tube_feedback", "Resonance", 0, 0.95, 0.01, "lin", "How strongly the cavity rings."],
		["tube_mix", "Mix", 0, 1, 0.01, "lin", "How much of the cavity is heard."],
	]},
	{ title: "Distortion", active: (v) => v.drive > 1 || v.clip < 1 || v.quantize > 1 || v.downsample > 1, fields: [
		["drive", "Drive", 0.05, 16, 0.05, "lin", "Pushes the signal into the clipper."],
		["clip", "Clip level", 0.05, 1, 0.01, "lin", "Ceiling of the clipper; lower is harsher."],
		["soft_clip", "Soft clip", "bool", 0, 0, "", "Rounded saturation instead of a hard edge."],
		["quantize", "Bit crush", 0, 2000, 1, "lin", "Quantisation step; higher is grittier and more digital."],
		["downsample", "Downsample", 1, 32, 1, "lin", "Holds every Nth sample; lowers fidelity and adds aliasing."],
	]},
	{ title: "Dynamics", active: (v) => v.compress_ratio > 1 || v.compress_makeup_db !== 0, fields: [
		["compress_threshold_db", "Threshold", -60, 0, 0.5, "lin", "Level above which the compressor squashes."],
		["compress_ratio", "Ratio", 1, 40, 0.1, "lin", "How hard it squashes. 10 and up behaves like a radio's automatic gain."],
		["compress_makeup_db", "Makeup", -24, 36, 0.5, "lin", "Gain after compression."],
	]},
	{ title: "Channel noise", active: (v) => v.hiss > 0 || v.crackle > 0 || v.dropout > 0 || v.noise > 0, fields: [
		["hiss", "Hiss", 0, 0.25, 0.001, "lin", "Steady static, shaped by the band filter."],
		["crackle", "Crackle", 0, 1, 0.01, "lin", "Static pops."],
		["dropout", "Dropouts", 0, 1, 0.01, "lin", "How often the voice fades under the static, like a weak signal."],
		["noise", "White noise", 0, 0.25, 0.001, "lin", "Unfiltered noise added at the very end."],
	]},
	{ title: "Key-up", active: (v) => v.squelch_level > 0 || v.chirp_level > 0, fields: [
		["squelch_ms", "Squelch length", 0, 1000, 1, "lin", "Static burst at the start of each transmission. Press K or start talking after a pause to hear it."],
		["squelch_level", "Squelch level", 0, 1, 0.01, "lin", "Loudness of the burst."],
		["chirp_hz", "Chirp pitch", 0, 8000, 1, "log", "Tone of the key-up beep."],
		["chirp_ms", "Chirp length", 0, 500, 1, "lin", "Length of each beep."],
		["chirp_level", "Chirp level", 0, 1, 0.01, "lin", "Loudness of the beeps."],
		["chirp_count", "Chirps", 1, 4, 1, "lin", "Number of beeps; alternate beeps drop in pitch."],
	]},
	{ title: "Filters", active: (v) => v.highpass_hz > 0 || v.lowpass_hz > 0 || v.band_low_hz > 0 || v.band_high_hz > 0 || v.peak1_db || v.peak2_db || v.peak3_db, fields: [
		["highpass_hz", "Gentle low cut", 0, 11000, 1, "log", "One-pole high-pass early in the chain."],
		["lowpass_hz", "Gentle high cut", 0, 11000, 1, "log", "One-pole low-pass early in the chain."],
		["band_low_hz", "Low cut", 0, 11000, 1, "log", "Steep high-pass: what a radio or speaker can't reproduce below."],
		["band_high_hz", "High cut", 0, 11000, 1, "log", "Steep low-pass: what a radio or speaker can't reproduce above."],
		["band_order", "Steepness", 1, 2, 1, "lin", "1 is 12 dB per octave, 2 is 24."],
		["peak1_hz", "Peak 1", 0, 11000, 1, "log", "Resonance centre, e.g. a mask cavity or a horn."],
		["peak1_db", "Peak 1 gain", -24, 24, 0.5, "lin", ""],
		["peak1_q", "Peak 1 width", 0.1, 24, 0.1, "lin", "Higher is narrower."],
		["peak2_hz", "Peak 2", 0, 11000, 1, "log", ""],
		["peak2_db", "Peak 2 gain", -24, 24, 0.5, "lin", ""],
		["peak2_q", "Peak 2 width", 0.1, 24, 0.1, "lin", ""],
		["peak3_hz", "Peak 3", 0, 11000, 1, "log", ""],
		["peak3_db", "Peak 3 gain", -24, 24, 0.5, "lin", ""],
		["peak3_q", "Peak 3 width", 0.1, 24, 0.1, "lin", ""],
	]},
	{ title: "Room", active: (v) => v.echo_mix > 0 || v.reverb_mix > 0, fields: [
		["echo_delay_ms", "Echo delay", 0, 500, 0.1, "lin", "Single reflection, e.g. a slapback off buildings."],
		["echo_mix", "Echo mix", 0, 0.95, 0.01, "lin", ""],
		["reverb_mix", "Reverb mix", 0, 1, 0.01, "lin", "Room around the voice."],
		["reverb_size", "Reverb size", 0, 1, 0.01, "lin", ""],
		["reverb_damp", "Reverb damping", 0, 1, 0.01, "lin", "Higher sounds softer, like furnished rooms."],
	]},
	{ title: "Output", active: () => true, fields: [
		["gain", "Gain", 0, 4, 0.01, "lin", "Final level."],
		["limit", "Limiter", "bool", 0, 0, "", "Soft limiter that keeps peaks under full scale."],
	]},
];
const FIELD = new Map(SECTIONS.flatMap((section) => section.fields.map((field) => [field[0], field])));

const $ = (id) => document.getElementById(id);
const audio = new AudioContext({ sampleRate: SAMPLE_RATE, latencyHint: "interactive" });

const state = {
	catalogue: null,
	stages: [],
	selected: 0,
	slots: {},
	activeSlot: null,
	library: [],
	collapsed: {},
	filter: "",
	undo: [],
	redo: [],
	clip: null,
	reference: null,
	source: "clip",
};
const nodes = { worklet: null, analyser: null, monitor: null, input: null, clipSource: null, micStream: null, reference: null };
let engine = null;
let lab = 0;
let historyTimer = 0;
let uid = 1;

/** Instantiates a WebAssembly engine with the WASI imports stubbed out (the engine never touches files). */
async function instantiateEngine(bytes) {
	const module = await WebAssembly.compile(bytes);
	const imports = {};
	for (const entry of WebAssembly.Module.imports(module)) (imports[entry.module] ??= {})[entry.name] = () => 0;
	const instance = await WebAssembly.instantiate(module, imports);
	instance.exports._initialize?.();
	return instance.exports;
}

function engineMessage() {
	const length = engine.vl_message_length(lab);
	return new TextDecoder().decode(new Uint8Array(engine.memory.buffer, engine.vl_message(lab), length));
}

function writeChain(target, handle, text) {
	const bytes = new TextEncoder().encode(text);
	const pointer = target.vl_text_buffer(handle, bytes.length);
	new Uint8Array(target.memory.buffer, pointer, bytes.length).set(bytes);
	return target.vl_set_chain(handle, bytes.length);
}

function parseGvox(text) {
	const values = {};
	for (const line of text.split(/\r?\n/)) {
		const at = line.indexOf("=");
		if (at < 0 || /^\s*[#;]/.test(line)) continue;
		const key = line.slice(0, at).trim().toLowerCase();
		const raw = line.slice(at + 1).trim();
		values[key] = raw === "true" ? true : raw === "false" ? false : raw !== "" && !isNaN(Number(raw)) ? Number(raw) : raw;
	}
	return values;
}

function toGvox(stage) {
	const lines = ["version=1", `name=${stage.name}`];
	if (stage.values.description) lines.push(`description=${stage.values.description}`);
	for (const key of FIELD.keys()) if (key in stage.values) lines.push(`${key}=${stage.values[key]}`);
	return lines.join("\n") + "\n";
}

function chainText(stages = state.stages) {
	return stages.filter((stage) => !stage.bypass).map(toGvox).join(STAGE_SEPARATOR);
}

function newStage(name, values, base = null) {
	return { uid: uid++, name, values: { ...values }, origin: { ...values }, base, bypass: false };
}

function stageFromPreset(id) {
	const preset = state.catalogue.presets[id];
	const values = parseGvox(preset.gvox);
	return newStage(preset.name, values, id);
}

/** Pushes the chain to the audio thread and validates it, then saves the session. */
function apply() {
	const text = chainText();
	nodes.worklet?.port.postMessage({ type: "chain", text });
	const result = writeChain(engine, lab, text);
	setStatus(result < 0 ? engineMessage() : "", result < 0);
	save();
}

function changed(options = {}) {
	if (!options.skipHistory) scheduleHistory();
	if (options.redraw) drawEditor();
	else refreshEditorState();
	drawChain();
	apply();
}

function snapshot() {
	return JSON.stringify({ stages: state.stages, selected: state.selected });
}

function scheduleHistory() {
	if (!historyTimer) state.undo.push(state.pendingSnapshot ?? snapshot());
	clearTimeout(historyTimer);
	historyTimer = setTimeout(() => { historyTimer = 0; state.pendingSnapshot = snapshot(); }, 400);
	state.redo = [];
	if (state.undo.length > 200) state.undo.shift();
}

function restoreSnapshot(text) {
	const data = JSON.parse(text);
	state.stages = data.stages;
	state.selected = Math.min(data.selected, state.stages.length - 1);
	state.pendingSnapshot = text;
	drawEditor();
	changed({ skipHistory: true, redraw: true });
}

function undo() {
	if (!state.undo.length) return;
	state.redo.push(snapshot());
	restoreSnapshot(state.undo.pop());
}

function redo() {
	if (!state.redo.length) return;
	state.undo.push(snapshot());
	restoreSnapshot(state.redo.pop());
}

function setStatus(text, bError = false) {
	$("status").textContent = text;
	$("status").classList.toggle("error", bError);
}

/** Slider position for a value, logarithmic for frequencies, with position 0 meaning off when the range starts at 0. */
function toSlider(field, value) {
	const [, , min, max, , scale] = field;
	if (scale !== "log") return Math.round((value - min) / (max - min) * SLIDER_STEPS);
	if (value <= 0) return 0;
	const low = Math.max(min, 20);
	return Math.round(Math.log(Math.max(value, low) / low) / Math.log(max / low) * SLIDER_STEPS);
}

function fromSlider(field, position) {
	const [, , min, max, step, scale] = field;
	if (scale !== "log") return roundStep(min + (max - min) * position / SLIDER_STEPS, step);
	if (position === 0 && min === 0) return 0;
	const low = Math.max(min, 20);
	return roundStep(low * Math.pow(max / low, position / SLIDER_STEPS), step);
}

function roundStep(value, step) {
	const decimals = step < 1 ? String(step).split(".")[1].length : 0;
	return Number((Math.round(value / step) * step).toFixed(decimals));
}

function defaultOf(field) {
	const [key, , min] = field;
	const defaults = { gain: 1, pitch: 1, drive: 1, clip: 1, downsample: 1, compress_ratio: 1, chirp_count: 1, band_order: 2,
		vocoder_bands: 16, vocoder_low_hz: 150, vocoder_high_hz: 4000, vocoder_width: 1, vocoder_attack_ms: 2,
		vocoder_release_ms: 20, vocoder_buzz: 1, vocoder_buzz_hz: 110, vocoder_gain: 1, vocoder_formant: 1,
		peak1_q: 1, peak2_q: 1, peak3_q: 1, reverb_size: 0.5, reverb_damp: 0.5 };
	if (key in defaults) return defaults[key];
	return min === "bool" ? false : 0;
}

function valueOf(stage, field) {
	return stage.values[field[0]] ?? defaultOf(field);
}

function drawChain() {
	const list = $("chain");
	list.innerHTML = "";
	if (!state.stages.length) list.innerHTML = '<div class="hint">Empty chain: the voice plays dry.</div>';
	state.stages.forEach((stage, index) => {
		const row = document.createElement("div");
		row.className = "stage" + (index === state.selected ? " selected" : "") + (stage.bypass ? " bypassed" : "");
		row.innerHTML = `<span class="index">${index + 1}</span><span class="name"></span>`;
		row.querySelector(".name").textContent = stage.name;
		row.onclick = () => { state.selected = index; drawChain(); drawEditor(); };
		const actions = document.createElement("span");
		actions.className = "actions";
		const add = (label, title, action) => {
			const button = document.createElement("button");
			button.textContent = label;
			button.title = title;
			button.onclick = (event) => { event.stopPropagation(); action(); };
			actions.appendChild(button);
		};
		add(stage.bypass ? "off" : "on", "Bypass this stage", () => { stage.bypass = !stage.bypass; changed(); });
		add("↑", "Move earlier", () => move(index, -1));
		add("↓", "Move later", () => move(index, 1));
		add("⧉", "Duplicate", () => {
			state.stages.splice(index + 1, 0, { ...structuredClone(stage), uid: uid++, name: stage.name + " copy" });
			state.selected = index + 1;
			changed({ redraw: true });
		});
		add("×", "Remove", () => {
			state.stages.splice(index, 1);
			state.selected = Math.max(0, Math.min(state.selected, state.stages.length - 1));
			changed({ redraw: true });
		});
		row.appendChild(actions);
		list.appendChild(row);
	});
}

function move(index, delta) {
	const target = index + delta;
	if (target < 0 || target >= state.stages.length) return;
	const [stage] = state.stages.splice(index, 1);
	state.stages.splice(target, 0, stage);
	state.selected = target;
	changed({ redraw: true });
}

/** Builds the editor for the selected stage: one collapsible card per section, in signal order. */
function drawEditor() {
	const editor = $("editor");
	editor.innerHTML = "";
	const stage = state.stages[state.selected];
	if (!stage) {
		editor.innerHTML = '<div class="empty">Add a preset or layer to the chain, or import a .gvox.</div>';
		return;
	}

	const head = document.createElement("div");
	head.className = "editor-head";
	head.innerHTML = `<input id="stage-name" spellcheck="false"><input id="stage-description" spellcheck="false" placeholder="Description">`;
	editor.appendChild(head);
	$("stage-name").value = stage.name;
	$("stage-description").value = stage.values.description || "";
	$("stage-name").oninput = (event) => { stage.name = event.target.value || "Custom"; drawChain(); save(); };
	$("stage-description").oninput = (event) => { stage.values.description = event.target.value; save(); };

	const tools = document.createElement("div");
	tools.className = "toolbar";
	const button = (label, title, action) => {
		const element = document.createElement("button");
		element.textContent = label;
		element.title = title;
		element.onclick = action;
		tools.appendChild(element);
	};
	button("Reset all", "Every field back to how this stage started", () => { stage.values = { ...stage.origin }; changed({ redraw: true }); });
	button("Copy .gvox", "Copy this stage as a preset file", () => { navigator.clipboard.writeText(toGvox(stage)); flash("Copied"); });
	button("Download .gvox", "Save this stage as a preset file", () => download(`${slug(stage.name)}.gvox`, new Blob([toGvox(stage)], { type: "text/plain" })));
	button("Save to library", "Keep this stage in the library", () => {
		state.library = state.library.filter((entry) => entry.name !== stage.name);
		state.library.unshift({ name: stage.name, gvox: toGvox(stage) });
		drawLibrary();
		save();
		flash("Saved to library");
	});
	button("Edit text", "Edit the raw .gvox", () => {
		const area = $("raw");
		area.hidden = !area.hidden;
		area.value = toGvox(stage);
	});
	const filter = document.createElement("input");
	filter.type = "search";
	filter.placeholder = "Find a control";
	filter.value = state.filter;
	filter.oninput = () => { state.filter = filter.value.toLowerCase(); applyFilter(); };
	tools.appendChild(filter);
	editor.appendChild(tools);

	const raw = document.createElement("textarea");
	raw.id = "raw";
	raw.hidden = true;
	raw.spellcheck = false;
	raw.onchange = () => {
		const values = parseGvox(raw.value);
		stage.name = values.name || stage.name;
		delete values.name;
		delete values.version;
		stage.values = values;
		changed({ redraw: true });
	};
	editor.appendChild(raw);

	const grid = document.createElement("div");
	grid.className = "sections";
	for (const section of SECTIONS) {
		const card = document.createElement("section");
		card.className = "card" + (state.collapsed[section.title] ? " collapsed" : "");
		card.dataset.title = section.title;
		const title = document.createElement("h3");
		title.innerHTML = `<span class="dot"></span>${section.title}`;
		title.onclick = () => {
			state.collapsed[section.title] = !state.collapsed[section.title];
			card.classList.toggle("collapsed");
			save();
		};
		card.appendChild(title);
		const body = document.createElement("div");
		body.className = "fields";
		for (const field of section.fields) body.appendChild(fieldRow(stage, field));
		card.appendChild(body);
		grid.appendChild(card);
	}
	editor.appendChild(grid);
	refreshEditorState();
	applyFilter();
}

function fieldRow(stage, field) {
	const [key, label, min, max, step, , help] = field;
	const row = document.createElement("div");
	row.className = "field";
	row.dataset.key = key;
	row.dataset.search = (label + " " + key + " " + help).toLowerCase();
	row.title = help ? `${help}\n\n${key}` : key;
	const name = document.createElement("label");
	name.textContent = label;
	row.appendChild(name);

	const reset = document.createElement("button");
	reset.className = "reset";
	reset.textContent = "↺";
	reset.title = "Back to how this stage started";
	reset.onclick = () => { setValue(stage, key, stage.origin[key] ?? defaultOf(field)); syncRow(row, stage, field); };

	if (min === "bool") {
		const box = document.createElement("input");
		box.type = "checkbox";
		box.onchange = () => setValue(stage, key, box.checked);
		row.append(box, document.createElement("span"), reset);
		syncRow(row, stage, field);
		return row;
	}

	const slider = document.createElement("input");
	slider.type = "range";
	slider.min = 0;
	slider.max = SLIDER_STEPS;
	slider.oninput = () => {
		const value = fromSlider(field, Number(slider.value));
		number.value = value;
		setValue(stage, key, value);
	};
	slider.ondblclick = () => reset.onclick();
	const number = document.createElement("input");
	number.type = "number";
	number.step = step;
	number.onchange = () => {
		const value = Math.min(max, Math.max(min, Number(number.value)));
		setValue(stage, key, value);
		syncRow(row, stage, field);
	};
	row.append(slider, number, reset);
	syncRow(row, stage, field);
	return row;
}

function syncRow(row, stage, field) {
	const value = valueOf(stage, field);
	const [, , min] = field;
	if (min === "bool") row.querySelector("input").checked = !!value;
	else {
		const [slider, number] = row.querySelectorAll("input");
		slider.value = toSlider(field, value);
		number.value = value;
	}
	row.classList.toggle("changed", value !== (stage.origin[field[0]] ?? defaultOf(field)));
}

function setValue(stage, key, value) {
	stage.values[key] = value;
	changed();
}

/** Lights the dot of every section that currently changes the sound, and marks edited fields. */
function refreshEditorState() {
	const stage = state.stages[state.selected];
	if (!stage) return;
	const values = Object.fromEntries([...FIELD.values()].map((field) => [field[0], valueOf(stage, field)]));
	for (const section of SECTIONS) {
		const card = document.querySelector(`.card[data-title="${section.title}"]`);
		card?.classList.toggle("active", !!section.active(values));
	}
	for (const row of document.querySelectorAll(".field")) {
		const field = FIELD.get(row.dataset.key);
		row.classList.toggle("changed", valueOf(stage, field) !== (stage.origin[field[0]] ?? defaultOf(field)));
	}
}

function applyFilter() {
	for (const row of document.querySelectorAll(".field")) row.hidden = state.filter && !row.dataset.search.includes(state.filter);
	for (const card of document.querySelectorAll(".card")) {
		card.hidden = state.filter && ![...card.querySelectorAll(".field")].some((row) => !row.hidden);
		if (state.filter) card.classList.remove("collapsed");
		else card.classList.toggle("collapsed", !!state.collapsed[card.dataset.title]);
	}
}

function drawSlots() {
	const box = $("slots");
	box.innerHTML = "";
	for (const slot of SLOTS) {
		const element = document.createElement("button");
		element.className = "slot" + (state.slots[slot] ? " filled" : "") + (state.activeSlot === slot ? " current" : "");
		element.innerHTML = `<b>${slot}</b><span></span>`;
		element.querySelector("span").textContent = state.slots[slot]?.label ?? "empty";
		element.title = "Click to recall, Shift+click (or Shift+number key) to store the current chain";
		element.onclick = (event) => (event.shiftKey || !state.slots[slot] ? storeSlot(slot) : recallSlot(slot));
		box.appendChild(element);
	}
}

function storeSlot(slot) {
	state.slots[slot] = { stages: structuredClone(state.stages), label: state.stages.map((stage) => stage.name).join(" + ") || "dry" };
	state.activeSlot = slot;
	drawSlots();
	save();
	flash(`Stored in ${slot}`);
}

function recallSlot(slot) {
	if (!state.slots[slot]) return;
	scheduleHistory();
	state.stages = structuredClone(state.slots[slot].stages).map((stage) => ({ ...stage, uid: uid++ }));
	state.selected = Math.min(state.selected, state.stages.length - 1);
	state.activeSlot = slot;
	drawSlots();
	changed({ skipHistory: true, redraw: true });
}

function drawLibrary() {
	const list = $("library");
	list.innerHTML = "";
	if (!state.library.length) list.innerHTML = '<div class="hint">Saved stages appear here.</div>';
	for (const entry of state.library) {
		const row = document.createElement("div");
		row.className = "entry";
		const name = document.createElement("span");
		name.textContent = entry.name;
		name.title = "Add to the chain";
		name.onclick = () => addStage(stageFromText(entry.gvox));
		const remove = document.createElement("button");
		remove.textContent = "×";
		remove.title = "Delete from the library";
		remove.onclick = () => { state.library = state.library.filter((item) => item !== entry); drawLibrary(); save(); };
		row.append(name, remove);
		list.appendChild(row);
	}
}

function stageFromText(text) {
	const values = parseGvox(text);
	const name = values.name || "Imported";
	delete values.name;
	delete values.version;
	return newStage(name, values);
}

function addStage(stage) {
	scheduleHistory();
	state.stages.push(stage);
	state.selected = state.stages.length - 1;
	changed({ skipHistory: true, redraw: true });
}

function flash(text) {
	const toast = $("toast");
	toast.textContent = text;
	toast.classList.add("show");
	clearTimeout(flash.timer);
	flash.timer = setTimeout(() => toast.classList.remove("show"), 1400);
}

function slug(text) {
	return text.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_|_$/g, "") || "preset";
}

function download(name, blob) {
	const link = document.createElement("a");
	link.href = URL.createObjectURL(blob);
	link.download = name;
	link.click();
}

function save() {
	const data = { stages: state.stages, selected: state.selected, slots: state.slots, activeSlot: state.activeSlot,
		library: state.library, collapsed: state.collapsed, source: state.source, pushToTalk: $("ptt").checked };
	localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
}

/* Audio graph and sources */

async function setupAudio(bytes) {
	await audio.audioWorklet.addModule("worklet.js");
	nodes.worklet = new AudioWorkletNode(audio, "voicelab", { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
	nodes.worklet.port.postMessage({ type: "engine", bytes });
	nodes.worklet.port.onmessage = (event) => {
		const message = event.data;
		if (message.type === "level") drawMeter(message.peak);
		else if (message.type === "keyed") pulse($("key"));
		else if (message.type === "error") setStatus(message.text, true);
	};
	nodes.analyser = audio.createAnalyser();
	nodes.analyser.fftSize = 1024;
	nodes.analyser.smoothingTimeConstant = 0;
	nodes.monitor = audio.createGain();
	nodes.worklet.connect(nodes.analyser).connect(nodes.monitor).connect(audio.destination);
	nodes.monitor.gain.value = Number($("volume").value);
}

function disconnectInput() {
	if (nodes.clipSource) { try { nodes.clipSource.stop(); } catch (error) { } nodes.clipSource.disconnect(); nodes.clipSource = null; }
	if (nodes.input) { nodes.input.disconnect(); nodes.input = null; }
	if (nodes.micStream) { nodes.micStream.getTracks().forEach((track) => track.stop()); nodes.micStream = null; }
	$("play").textContent = "Play";
	$("play").classList.remove("live");
}

async function startMic() {
	disconnectInput();
	const deviceId = $("device").value || undefined;
	nodes.micStream = await navigator.mediaDevices.getUserMedia({
		audio: { deviceId, echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1 },
	});
	nodes.input = audio.createMediaStreamSource(nodes.micStream);
	nodes.input.connect(nodes.worklet);
	await audio.resume();
	$("play").textContent = "Stop mic";
	$("play").classList.add("live");
	listDevices();
}

async function startClip() {
	disconnectInput();
	if (!state.clip) { flash("Load or record a clip first"); return; }
	nodes.clipSource = audio.createBufferSource();
	nodes.clipSource.buffer = state.clip;
	nodes.clipSource.loop = $("loop").checked;
	nodes.clipSource.onended = () => { if (nodes.clipSource && !nodes.clipSource.loop) disconnectInput(); };
	nodes.clipSource.connect(nodes.worklet);
	nodes.worklet.port.postMessage({ type: "begin" });
	nodes.clipSource.start();
	await audio.resume();
	$("play").textContent = "Stop";
	$("play").classList.add("live");
}

function togglePlay() {
	if (nodes.clipSource || nodes.micStream) { disconnectInput(); return; }
	if (state.source === "mic") startMic().catch((error) => setStatus("Microphone: " + error.message, true));
	else startClip();
}

async function listDevices() {
	const devices = (await navigator.mediaDevices.enumerateDevices()).filter((device) => device.kind === "audioinput");
	const select = $("device");
	const current = select.value;
	select.innerHTML = '<option value="">Default microphone</option>';
	for (const device of devices) {
		if (!device.deviceId) continue;
		const option = document.createElement("option");
		option.value = device.deviceId;
		option.textContent = device.label || "Microphone";
		select.appendChild(option);
	}
	select.value = current;
}

/** Decodes any audio file and resamples it to 24 kHz mono, optionally normalised. */
async function decodeClip(arrayBuffer) {
	const decoded = await audio.decodeAudioData(arrayBuffer);
	const offline = new OfflineAudioContext(1, Math.ceil(decoded.duration * SAMPLE_RATE), SAMPLE_RATE);
	const source = offline.createBufferSource();
	source.buffer = decoded;
	source.connect(offline.destination);
	source.start();
	const rendered = await offline.startRendering();
	if ($("normalise").checked) {
		const data = rendered.getChannelData(0);
		let peak = 0;
		for (const value of data) peak = Math.max(peak, Math.abs(value));
		if (peak > 0) for (let i = 0; i < data.length; i++) data[i] *= 0.6 / peak;
	}
	return rendered;
}

async function useClip(buffer, label) {
	state.clip = buffer;
	$("clip-label").textContent = `${label} · ${buffer.duration.toFixed(1)} s`;
	const wav = encodeWav(buffer.getChannelData(0));
	try { localStorage.setItem(STORAGE_KEY + ".clip", await blobToBase64(wav)); } catch (error) { }
	setSource("clip");
}

let recorder = null;

async function toggleRecord() {
	if (recorder) { recorder.stop(); return; }
	const stream = await navigator.mediaDevices.getUserMedia({
		audio: { deviceId: $("device").value || undefined, echoCancellation: false, noiseSuppression: false, autoGainControl: false },
	});
	const chunks = [];
	recorder = new MediaRecorder(stream);
	recorder.ondataavailable = (event) => chunks.push(event.data);
	recorder.onstop = async () => {
		stream.getTracks().forEach((track) => track.stop());
		recorder = null;
		$("record").textContent = "Record clip";
		$("record").classList.remove("live");
		useClip(await decodeClip(await new Blob(chunks).arrayBuffer()), "Recording");
	};
	recorder.start();
	$("record").textContent = "Stop recording";
	$("record").classList.add("live");
}

function setSource(source) {
	const wasPlaying = !!(nodes.clipSource || nodes.micStream);
	state.source = source;
	for (const element of document.querySelectorAll("[data-source]")) element.classList.toggle("on", element.dataset.source === source);
	save();
	if (wasPlaying) togglePlay(), togglePlay();
}

/** Renders the loaded clip through the current chain in 20 ms packets, as the server would, and downloads it. */
function exportClip() {
	if (!state.clip) { flash("Load or record a clip first"); return; }
	const input = state.clip.getChannelData(0);
	const exporter = engine.vl_create();
	if (writeChain(engine, exporter, chainText()) < 0) { engine.vl_destroy(exporter); return; }
	engine.vl_begin(exporter);
	const output = new Float32Array(input.length);
	const packet = 480;
	for (let offset = 0; offset < input.length; offset += packet) {
		const count = Math.min(packet, input.length - offset);
		const pointer = engine.vl_audio_buffer(exporter, count);
		new Float32Array(engine.memory.buffer, pointer, count).set(input.subarray(offset, offset + count));
		engine.vl_process(exporter, count);
		output.set(new Float32Array(engine.memory.buffer, engine.vl_audio_buffer(exporter, count), count), offset);
	}
	engine.vl_destroy(exporter);
	download((state.stages.map((stage) => slug(stage.name)).join("+") || "dry") + ".wav", encodeWav(output));
}

function encodeWav(samples) {
	const buffer = new ArrayBuffer(44 + samples.length * 2);
	const view = new DataView(buffer);
	const text = (offset, value) => [...value].forEach((character, i) => view.setUint8(offset + i, character.charCodeAt(0)));
	text(0, "RIFF"); view.setUint32(4, 36 + samples.length * 2, true); text(8, "WAVEfmt ");
	view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
	view.setUint32(24, SAMPLE_RATE, true); view.setUint32(28, SAMPLE_RATE * 2, true); view.setUint16(32, 2, true);
	view.setUint16(34, 16, true); text(36, "data"); view.setUint32(40, samples.length * 2, true);
	const pcm = new Int16Array(buffer, 44);
	for (let i = 0; i < samples.length; i++) pcm[i] = Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767)));
	return new Blob([buffer], { type: "audio/wav" });
}

function blobToBase64(blob) {
	return new Promise((resolve) => {
		const reader = new FileReader();
		reader.onload = () => resolve(reader.result.split(",")[1]);
		reader.readAsDataURL(blob);
	});
}

async function playReference() {
	if (nodes.reference) { nodes.reference.stop(); nodes.reference = null; $("ref-play").classList.remove("live"); return; }
	if (!state.reference) { flash("Load a reference first"); return; }
	nodes.reference = audio.createBufferSource();
	nodes.reference.buffer = state.reference;
	nodes.reference.connect(nodes.analyser);
	nodes.reference.onended = () => { nodes.reference = null; $("ref-play").classList.remove("live"); };
	nodes.reference.start();
	$("ref-play").classList.add("live");
	await audio.resume();
}

/* Meters and spectrogram */

function drawMeter(peak) {
	const db = 20 * Math.log10(peak + 1e-9);
	$("meter-fill").style.width = `${Math.max(0, Math.min(100, (db + 60) / 60 * 100))}%`;
	$("meter").classList.toggle("hot", peak > 0.98);
}

function pulse(element) {
	element.classList.add("pulse");
	setTimeout(() => element.classList.remove("pulse"), 250);
}

function startSpectrogram() {
	const canvas = $("spectrogram");
	const context = canvas.getContext("2d");
	const bins = new Float32Array(nodes.analyser.frequencyBinCount);
	const frame = () => {
		requestAnimationFrame(frame);
		const width = canvas.width;
		const height = canvas.height;
		context.drawImage(canvas, -2, 0);
		nodes.analyser.getFloatFrequencyData(bins);
		const nyquist = SAMPLE_RATE / 2;
		for (let y = 0; y < height; y++) {
			const hz = 60 * Math.pow(nyquist / 60, 1 - y / height);
			const level = Math.max(0, Math.min(1, (bins[Math.min(bins.length - 1, Math.round(hz / nyquist * bins.length))] + 100) / 70));
			context.fillStyle = `rgba(${40 + level * 90}, ${70 + level * 150}, ${110 + level * 145}, ${0.15 + level * 0.85})`;
			context.fillRect(width - 2, y, 2, 1);
		}
	};
	frame();
}

/* Keyboard */

function bindKeys() {
	document.addEventListener("keydown", (event) => {
		if (event.target.matches("input:not([type=range]):not([type=checkbox]), textarea, select")) return;
		if ((event.ctrlKey || event.metaKey) && event.code === "KeyZ") { event.preventDefault(); event.shiftKey ? redo() : undo(); return; }
		if ((event.ctrlKey || event.metaKey) && event.code === "KeyY") { event.preventDefault(); redo(); return; }
		if (event.repeat) return;
		if (event.code === "Space") { event.preventDefault(); togglePlay(); }
		else if (event.code === "KeyB") { nodes.worklet?.port.postMessage({ type: "bypass", value: true }); $("dry").classList.add("live"); }
		else if (event.code === "KeyT") { nodes.worklet?.port.postMessage({ type: "talk", value: true }); $("talk").classList.add("live"); }
		else if (event.code === "KeyK") nodes.worklet?.port.postMessage({ type: "begin" });
		else if (event.code === "KeyR") playReference();
		else if (/^Digit[1-4]$/.test(event.code)) {
			const slot = SLOTS[Number(event.code.slice(5)) - 1];
			event.shiftKey ? storeSlot(slot) : recallSlot(slot);
		}
	});
	document.addEventListener("keyup", (event) => {
		if (event.code === "KeyB") { nodes.worklet?.port.postMessage({ type: "bypass", value: false }); $("dry").classList.remove("live"); }
		else if (event.code === "KeyT") { nodes.worklet?.port.postMessage({ type: "talk", value: false }); $("talk").classList.remove("live"); }
	});
}

function bindHold(element, type) {
	const set = (value) => { nodes.worklet?.port.postMessage({ type, value }); element.classList.toggle("live", value); };
	element.onpointerdown = () => set(true);
	element.onpointerup = element.onpointerleave = () => set(false);
}

/* Start-up */

async function restore() {
	let data = null;
	try { data = JSON.parse(localStorage.getItem(STORAGE_KEY)); } catch (error) { }
	if (data?.stages) {
		state.stages = data.stages.map((stage) => ({ ...stage, uid: uid++ }));
		state.selected = Math.min(data.selected ?? 0, Math.max(0, state.stages.length - 1));
		state.slots = data.slots ?? {};
		state.activeSlot = data.activeSlot ?? null;
		state.library = data.library ?? [];
		state.collapsed = data.collapsed ?? {};
		state.source = data.source ?? "clip";
		$("ptt").checked = !!data.pushToTalk;
	} else {
		state.stages = [stageFromPreset(15)];
	}
	const clip = localStorage.getItem(STORAGE_KEY + ".clip");
	if (clip) {
		const bytes = Uint8Array.from(atob(clip), (character) => character.charCodeAt(0));
		state.clip = await decodeClip(bytes.buffer);
		$("clip-label").textContent = `Last clip · ${state.clip.duration.toFixed(1)} s`;
	}
}

async function start() {
	const bytes = await (await fetch("engine.wasm")).arrayBuffer();
	engine = await instantiateEngine(bytes.slice(0));
	lab = engine.vl_create();
	const length = engine.vl_catalogue(lab);
	state.catalogue = JSON.parse(engineMessage().slice(0, length));

	for (const preset of state.catalogue.presets) {
		if (preset.id === 0) continue;
		$("add-preset").insertAdjacentHTML("beforeend", `<option value="${preset.id}"></option>`);
		$("add-preset").lastElementChild.textContent = preset.name;
		$("add-preset").lastElementChild.title = preset.description;
	}
	for (const layer of state.catalogue.layers) {
		$("add-layer").insertAdjacentHTML("beforeend", `<option value="${layer.preset}"></option>`);
		$("add-layer").lastElementChild.textContent = layer.name;
	}
	for (const id of ["add-preset", "add-layer"]) {
		$(id).onchange = (event) => {
			if (!event.target.value) return;
			addStage(stageFromPreset(Number(event.target.value)));
			event.target.value = "";
		};
	}

	await setupAudio(bytes);
	await restore();
	state.pendingSnapshot = snapshot();
	nodes.worklet.port.postMessage({ type: "pushToTalk", value: $("ptt").checked });
	setSource(state.source);
	drawChain();
	drawEditor();
	drawSlots();
	drawLibrary();
	apply();
	startSpectrogram();
	listDevices().catch(() => { });
	bindKeys();

	$("play").onclick = togglePlay;
	$("record").onclick = () => toggleRecord().catch((error) => setStatus("Microphone: " + error.message, true));
	$("load").onchange = async (event) => {
		const file = event.target.files[0];
		if (file) useClip(await decodeClip(await file.arrayBuffer()), file.name);
		event.target.value = "";
	};
	$("ref-load").onchange = async (event) => {
		const file = event.target.files[0];
		if (!file) return;
		state.reference = await decodeClip(await file.arrayBuffer());
		$("ref-label").textContent = file.name;
		event.target.value = "";
	};
	$("ref-play").onclick = playReference;
	for (const element of document.querySelectorAll("[data-source]")) element.onclick = () => setSource(element.dataset.source);
	$("ptt").onchange = () => { nodes.worklet.port.postMessage({ type: "pushToTalk", value: $("ptt").checked }); save(); };
	$("volume").oninput = () => { nodes.monitor.gain.value = Number($("volume").value); };
	$("loop").onchange = () => { if (nodes.clipSource) nodes.clipSource.loop = $("loop").checked; };
	$("key").onclick = () => nodes.worklet.port.postMessage({ type: "begin" });
	bindHold($("dry"), "bypass");
	bindHold($("talk"), "talk");
	$("export").onclick = exportClip;
	$("import").onchange = async (event) => {
		for (const file of event.target.files) addStage(stageFromText(await file.text()));
		event.target.value = "";
	};
	$("paste").onclick = async () => {
		const text = await navigator.clipboard.readText().catch(() => "");
		if (!text.includes("=")) { flash("Clipboard has no .gvox text"); return; }
		addStage(stageFromText(text));
	};
	document.addEventListener("dragover", (event) => event.preventDefault());
	document.addEventListener("drop", async (event) => {
		event.preventDefault();
		for (const file of event.dataTransfer.files) {
			if (/\.gvox$/i.test(file.name) || file.type.startsWith("text")) addStage(stageFromText(await file.text()));
			else if (file.type.startsWith("audio")) useClip(await decodeClip(await file.arrayBuffer()), file.name);
		}
	});
	$("loading").remove();
}

start().catch((error) => {
	$("loading").textContent = "Could not start the engine: " + error.message;
	console.error(error);
});
