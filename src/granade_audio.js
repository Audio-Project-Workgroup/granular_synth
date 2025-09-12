class GranadeProcessor extends AudioWorkletProcessor {
    constructor() {
	super();
	this.phase = 0;
	this.freq = 440.0;
	this.volume = 0.1;
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
