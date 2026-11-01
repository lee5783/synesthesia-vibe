//
//  audio-input-worklet.js
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

const CHANNELS = 2;
const DEFAULT_BLOCK_FRAMES = 512;

class SynesthesiaAudioInputProcessor extends AudioWorkletProcessor {

    constructor(options) {
        super();
        const parameters = options.processorOptions ?? {};
        this.targetRate = parameters.targetRate ?? sampleRate;
        this.blockFrames = parameters.blockFrames ?? DEFAULT_BLOCK_FRAMES;
        this.step = sampleRate / this.targetRate;
        this.resampling = Math.abs(this.step - 1) > 1e-9;
        this.phase = 0;
        this.previousLeft = 0;
        this.previousRight = 0;
        this.block = new Float32Array(this.blockFrames * CHANNELS);
        this.filled = 0;
        this.running = true;
        this.port.onmessage = event => {
            if (event.data === "stop")
                this.running = false;
        };
    }

    process(inputs) {
        const input = inputs[0];

        if (input === undefined || input.length === 0)
            return this.running;

        const left = input[0];
        const right = input.length > 1 ? input[1] : input[0];

        if (this.resampling)
            this.resample(left, right);
        else
            for (let i = 0; i < left.length; i++)
                this.emit(left[i], right[i]);

        return this.running;
    }

    resample(left, right) {
        for (let i = 0; i < left.length; i++) {
            const currentLeft = left[i];
            const currentRight = right[i];

            while (this.phase < 1) {
                this.emit(this.previousLeft + (currentLeft - this.previousLeft) * this.phase,
                    this.previousRight + (currentRight - this.previousRight) * this.phase);
                this.phase += this.step;
            }

            this.phase -= 1;
            this.previousLeft = currentLeft;
            this.previousRight = currentRight;
        }
    }

    emit(leftSample, rightSample) {
        this.block[this.filled * CHANNELS] = leftSample;
        this.block[this.filled * CHANNELS + 1] = rightSample;
        this.filled += 1;

        if (this.filled < this.blockFrames)
            return;

        this.port.postMessage(this.block, [this.block.buffer]);
        this.block = new Float32Array(this.blockFrames * CHANNELS);
        this.filled = 0;
    }
}

registerProcessor("synesthesia-audio-input", SynesthesiaAudioInputProcessor);
