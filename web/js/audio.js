// Small WebAudio wrapper. Buffers are loaded lazily after the first gesture so
// browsers do not block the context.

const FILES = {
    cue: 'sounds/click.wav',
    click: 'sounds/click.wav',
    rail: 'sounds/rail.wav',
    pocket: 'sounds/pocket.wav',
};

const VOLUME = {
    cue: 0.35,
    click: 0.45,
    rail: 0.3,
    pocket: 0.5,
};

export class Sound {
    constructor() {
        this.context = null;
        this.buffers = new Map();
        this.loading = null;
        this.muted = false;
    }

    async unlock() {
        if (!this.context) {
            const Context = window.AudioContext || window.webkitAudioContext;
            if (!Context)
                return;
            this.context = new Context();
        }
        if (this.context.state === 'suspended')
            await this.context.resume().catch(() => {});
        if (!this.loading)
            this.loading = this.load();
        await this.loading;
    }

    async load() {
        await Promise.all(Object.entries(FILES).map(async ([kind, url]) => {
            try {
                const response = await fetch(url);
                if (!response.ok)
                    return;
                const data = await response.arrayBuffer();
                const buffer = await this.context.decodeAudioData(data);
                this.buffers.set(kind, buffer);
            } catch {
                // Sound is a nicety; the game works without it.
            }
        }));
    }

    play(kind) {
        if (this.muted || !this.context || this.context.state !== 'running')
            return;
        const buffer = this.buffers.get(kind);
        if (!buffer)
            return;

        const source = this.context.createBufferSource();
        source.buffer = buffer;
        const gain = this.context.createGain();
        gain.gain.value = VOLUME[kind] ?? 0.4;
        source.connect(gain);
        gain.connect(this.context.destination);
        source.start();
    }

    setMuted(muted) {
        this.muted = muted;
    }
}
