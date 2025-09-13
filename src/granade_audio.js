function platformLog(msgPtr) { msgPtr = null; }

class GranadeProcessor extends AudioWorkletProcessor {
    constructor(options) {
	super(options);
	this.phase = 0;
	this.freq = 440.0;
	this.volume = 0.1;
	this.sharedMemory = options.processorOptions.sharedMemory;
	this.wasmModule = options.processorOptions.wasmModule;
	this.wasmInstance = await WebAssembly.instantiate(this.wasmModule, {
	    env: {
		memory: this.sharedMemory,
		platformLog,
	    }
	});
	console.log(this.wasmInstance); // ?
    }

    process(inputs, outputs, parameters) {
	const output = outputs[0];
	const samplePeriod = 1.0 / sampleRate;
	const tau = 2.0 * Math.PI;
	const phaseInc = tau * this.freq * samplePeriod;

	output.forEach((channel) => {
	    for(let sampleIdx = 0; sampleIdx < channel.length; ++sampleIdx) {
		channel[sampleIdx] = this.volume * Math.sin(this.phase);

		this.phase += phaseInc;
		if(this.phase >= tau) {
		    this.phase -= tau;
		}
	    }
	});

	return true;
    }
}

registerProcessor("granade-processor", GranadeProcessor);
