// A gentle, endless jazz trio generated with WebAudio. There is no music file
// to download: a ii-V-I progression, soft electric piano, upright bass, and
// brushed ride cymbals are scheduled a little ahead of time. The result loops
// without a seam and stays quiet under the game.

const TEMPO = 74;
const STEPS_PER_BEAT = 2;
const BEATS_PER_BAR = 4;
const STEPS_PER_BAR = STEPS_PER_BEAT * BEATS_PER_BAR;
const STEP_SECONDS = 60 / TEMPO / STEPS_PER_BEAT;
const SWING = STEP_SECONDS * 0.16;
const LOOKAHEAD = 0.6;

// Eight bars in F major. Each chord is a rootless voicing; the bass carries
// the root so the piano can stay light and open.
const BARS = [
    { bass: [41, 48, 45, 40], chord: [53, 57, 60, 64] },   // Fmaj9
    { bass: [38, 45, 41, 42], chord: [53, 57, 60, 64] },   // Dm9
    { bass: [43, 50, 46, 47], chord: [58, 62, 65, 69] },   // Gm9
    { bass: [36, 43, 40, 41], chord: [52, 55, 58, 62] },   // C13
    { bass: [45, 52, 48, 44], chord: [55, 60, 64, 67] },   // Am9
    { bass: [38, 45, 41, 42], chord: [53, 57, 60, 64] },   // Dm9
    { bass: [43, 50, 46, 47], chord: [58, 62, 65, 69] },   // Gm9
    { bass: [36, 43, 40, 41], chord: [52, 55, 58, 62] },   // C13
];

// Sparse phrases that leave plenty of space.
const MELODY = [
    [0, 2, 69, 1.7], [0, 5, 72, 1.3],
    [1, 3, 77, 1.4], [1, 6, 76, 1.2],
    [2, 2, 74, 1.5], [2, 5, 70, 1.4],
    [3, 4, 67, 1.8],
    [4, 3, 76, 1.3], [4, 6, 72, 1.2],
    [5, 2, 69, 1.5],
    [6, 4, 70, 1.4], [6, 7, 74, 1.2],
    [7, 3, 67, 1.7],
];

const midiToHz = (midi) => 440 * 2 ** ((midi - 69) / 12);

function noiseBuffer(context, seconds) {
    const length = Math.floor(context.sampleRate * seconds);
    const buffer = context.createBuffer(1, length, context.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < length; i += 1)
        data[i] = Math.random() * 2 - 1;
    return buffer;
}

function impulse(context, seconds) {
    const length = Math.floor(context.sampleRate * seconds);
    const buffer = context.createBuffer(2, length, context.sampleRate);
    for (let channel = 0; channel < 2; channel += 1) {
        const data = buffer.getChannelData(channel);
        for (let i = 0; i < length; i += 1) {
            const decay = (1 - i / length) ** 2.4;
            data[i] = (Math.random() * 2 - 1) * decay;
        }
    }
    return buffer;
}

export class Jazz {
    constructor() {
        this.context = null;
        this.master = null;
        this.dry = null;
        this.reverb = null;
        this.noise = null;
        this.timer = null;
        this.step = 0;
        this.nextNoteTime = 0;
        this.enabled = true;
        this.started = false;
    }

    async unlock() {
        if (!this.context) {
            const Context = window.AudioContext || window.webkitAudioContext;
            if (!Context)
                return;
            this.context = new Context();
            this._build();
        }
        if (this.context.state === 'suspended')
            await this.context.resume().catch(() => {});
        if (this.enabled && !this.started)
            this.start();
    }

    _build() {
        const context = this.context;

        this.master = context.createGain();
        this.master.gain.value = this.enabled ? 0.8 : 0.0001;

        const compressor = context.createDynamicsCompressor();
        compressor.threshold.value = -22;
        compressor.knee.value = 22;
        compressor.ratio.value = 3;
        compressor.attack.value = 0.02;
        compressor.release.value = 0.35;

        this.master.connect(compressor);
        compressor.connect(context.destination);

        this.dry = context.createGain();
        this.dry.gain.value = 0.9;
        this.dry.connect(this.master);

        const convolver = context.createConvolver();
        convolver.buffer = impulse(context, 2.8);
        this.reverb = context.createGain();
        this.reverb.gain.value = 0.34;
        this.dry.connect(convolver);
        convolver.connect(this.reverb);
        this.reverb.connect(this.master);

        this.noise = noiseBuffer(context, 1);
    }

    start() {
        if (!this.context || this.started)
            return;
        this.started = true;
        this.nextNoteTime = this.context.currentTime + 0.12;
        this.timer = setInterval(() => this._schedule(), 200);
    }

    stop() {
        clearInterval(this.timer);
        this.timer = null;
        this.started = false;
    }

    setEnabled(enabled) {
        this.enabled = Boolean(enabled);
        if (!this.context)
            return;
        const now = this.context.currentTime;
        this.master.gain.cancelScheduledValues(now);
        this.master.gain.setValueAtTime(Math.max(0.0001, this.master.gain.value), now);
        if (this.enabled) {
            this.master.gain.exponentialRampToValueAtTime(0.8, now + 0.4);
            if (!this.started)
                this.start();
        } else {
            this.master.gain.exponentialRampToValueAtTime(0.0001, now + 0.35);
            setTimeout(() => {
                if (!this.enabled)
                    this.stop();
            }, 400);
        }
    }

    _schedule() {
        if (!this.context || !this.enabled)
            return;

        const now = this.context.currentTime;
        // A hidden tab throttles timers. Skip the gap instead of playing it.
        if (this.nextNoteTime < now - 0.3)
            this.nextNoteTime = now + 0.05;

        while (this.nextNoteTime < now + LOOKAHEAD) {
            this._scheduleStep(this.step, this.nextNoteTime);
            this.nextNoteTime += STEP_SECONDS;
            this.step = (this.step + 1) % (STEPS_PER_BAR * BARS.length);
        }
    }

    _scheduleStep(step, time) {
        const offset = step % 2 === 1 ? SWING : 0;
        const at = time + offset;
        const bar = Math.floor(step / STEPS_PER_BAR);
        const beat = Math.floor((step % STEPS_PER_BAR) / STEPS_PER_BEAT);
        const chord = BARS[bar];
        const inBar = step % STEPS_PER_BAR;

        // Upright bass: one note per beat, with a light approach note.
        if (inBar % STEPS_PER_BEAT === 0) {
            this._bass(at, chord.bass[beat], 0.9 + (beat === 0 ? 0.25 : 0));
        }

        // Piano comping: a soft chord on beat one, then a small answer.
        if (inBar === 0) {
            for (let i = 0; i < chord.chord.length; i += 1)
                this._piano(at + i * 0.012, chord.chord[i], 2.8, 0.028);
        } else if (inBar === 3) {
            const upper = chord.chord.slice(1);
            for (let i = 0; i < upper.length; i += 1)
                this._piano(at + i * 0.01, upper[i], 2.1, 0.018);
        }

        // Ride pattern with a brushed backbeat.
        this._ride(at, inBar % 2 === 0 ? 0.022 : 0.012);
        if (inBar === 2 || inBar === 6)
            this._brush(at, 0.05);
        if (inBar === 0)
            this._kick(at);

        // A few melody notes from the written phrase.
        for (const [phraseBar, phraseStep, midi, duration] of MELODY) {
            if (phraseBar === bar && phraseStep === inBar)
                this._melody(at, midi, duration);
        }
    }

    _piano(time, midi, duration, gainValue) {
        const context = this.context;
        const frequency = midiToHz(midi);

        const filter = context.createBiquadFilter();
        filter.type = 'lowpass';
        filter.frequency.value = 2100;
        filter.Q.value = 0.4;

        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(gainValue, time + 0.014);
        gain.gain.exponentialRampToValueAtTime(gainValue * 0.32, time + 0.28);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);

        const fundamental = context.createOscillator();
        fundamental.type = 'triangle';
        fundamental.frequency.value = frequency;
        fundamental.detune.value = -3;

        const shimmer = context.createOscillator();
        shimmer.type = 'sine';
        shimmer.frequency.value = frequency * 2.002;
        const shimmerGain = context.createGain();
        shimmerGain.gain.value = 0.14;

        fundamental.connect(filter);
        shimmer.connect(shimmerGain);
        shimmerGain.connect(filter);
        filter.connect(gain);
        gain.connect(this.dry);

        const end = time + duration + 0.05;
        fundamental.start(time);
        shimmer.start(time);
        fundamental.stop(end);
        shimmer.stop(end);
    }

    _bass(time, midi, duration) {
        const context = this.context;
        const frequency = midiToHz(midi);

        const filter = context.createBiquadFilter();
        filter.type = 'lowpass';
        filter.frequency.value = 420;
        filter.Q.value = 0.5;

        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(0.08, time + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.045, time + 0.18);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);

        const osc = context.createOscillator();
        osc.type = 'sine';
        osc.frequency.value = frequency;

        const body = context.createOscillator();
        body.type = 'triangle';
        body.frequency.value = frequency;
        const bodyGain = context.createGain();
        bodyGain.gain.value = 0.22;

        osc.connect(filter);
        body.connect(bodyGain);
        bodyGain.connect(filter);
        filter.connect(gain);
        gain.connect(this.dry);

        const end = time + duration + 0.05;
        osc.start(time);
        body.start(time);
        osc.stop(end);
        body.stop(end);
    }

    _melody(time, midi, duration) {
        const context = this.context;
        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(0.034, time + 0.03);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);

        const filter = context.createBiquadFilter();
        filter.type = 'lowpass';
        filter.frequency.value = 3200;

        const osc = context.createOscillator();
        osc.type = 'sine';
        osc.frequency.value = midiToHz(midi);

        osc.connect(filter);
        filter.connect(gain);
        gain.connect(this.dry);
        gain.connect(this.reverb);

        osc.start(time);
        osc.stop(time + duration + 0.05);
    }

    _ride(time, level) {
        const context = this.context;
        const source = context.createBufferSource();
        source.buffer = this.noise;

        const filter = context.createBiquadFilter();
        filter.type = 'highpass';
        filter.frequency.value = 4600;
        filter.Q.value = 0.6;

        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(level, time + 0.006);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.075);

        source.connect(filter);
        filter.connect(gain);
        gain.connect(this.dry);
        gain.connect(this.reverb);
        source.start(time, Math.random() * 0.5, 0.09);
    }

    _brush(time, level) {
        const context = this.context;
        const source = context.createBufferSource();
        source.buffer = this.noise;

        const filter = context.createBiquadFilter();
        filter.type = 'bandpass';
        filter.frequency.value = 1600;
        filter.Q.value = 0.7;

        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(level, time + 0.015);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.22);

        source.connect(filter);
        filter.connect(gain);
        gain.connect(this.dry);
        gain.connect(this.reverb);
        source.start(time, Math.random() * 0.4, 0.24);
    }

    _kick(time) {
        const context = this.context;
        const gain = context.createGain();
        gain.gain.setValueAtTime(0.0001, time);
        gain.gain.exponentialRampToValueAtTime(0.055, time + 0.008);
        gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.18);

        const osc = context.createOscillator();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(110, time);
        osc.frequency.exponentialRampToValueAtTime(46, time + 0.13);

        osc.connect(gain);
        gain.connect(this.dry);
        osc.start(time);
        osc.stop(time + 0.2);
    }
}
