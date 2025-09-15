function platformLog(msgPtr) { msgPtr = null; }

function platformSin(val) {
    return Math.sin(val);
}

function platformCos(val) {
    return Math.cos(val);
}

class GranadeProcessor extends AudioWorkletProcessor {
    constructor(options) {
	super(options);
	this.phase = 0;
	this.freq = 440.0;
	this.volume = 0.1;
	this.sharedMemory = options.processorOptions.sharedMemory;
	this.wasmModule = options.processorOptions.wasmModule;
	WebAssembly.instantiate(this.wasmModule, {
	    env: {
		memory: this.sharedMemory,
		platformLog,
		platformSin,
		platformCos,
	    }
	})
	    .then(instance => {
		this.wasmInstance = instance;
	    })
	    .catch(error => console.log("error instantiating wasm in audio processor: ", error));
    }

    process(inputs, outputs, parameters) {
	const output = outputs[0];
	const samplePeriod = 1.0 / sampleRate;
	const tau = 2.0 * Math.PI;
	const phaseInc = tau * this.freq * samplePeriod;

	const outputChannelCount = output.length;
	const outputSampleCount = output[0].length;

	for(let channelIdx = 0; channelIdx < outputChannelCount; ++channelIdx) {
	    const samplesOffset = this.wasmInstance.exports.outputSamples(channelIdx, outputSampleCount, samplePeriod);
	    const samples = new Float32Array(this.sharedMemory.buffer, samplesOffset, outputSampleCount);
	    for(let sampleIdx = 0; sampleIdx < outputSampleCount; ++sampleIdx) {
		output[channelIdx][sampleIdx] = samples[sampleIdx];
	    }
	}

	return true;
    }
}

registerProcessor("granade-processor", GranadeProcessor);
