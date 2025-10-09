import { makeImportObject } from './wasm_imports.js';

class GranadeProcessor extends AudioWorkletProcessor {
    constructor(options) {
	super(options);
	this.sharedMemory = options.processorOptions.sharedMemory;	
	this.wasmModule = options.processorOptions.wasmModule;
	this.importObject = makeImportObject(this.sharedMemory, this.port);
	WebAssembly.instantiate(this.wasmModule, this.importObject)
	    .then(instance => {
		this.wasmInstance = instance;
	    })
	    .catch(error => console.log("error instantiating wasm in audio processor: ", error));
    }

    process(inputs, outputs, parameters) {

	const input = inputs[0];
	const output = outputs[0];

	const inputChannelCount = input.length;
	// console.log(input);
	// console.log(inputChannelCount);
	const inputSampleCount = input[0].length;

	const outputChannelCount = output.length;
	const outputSampleCount = output[0].length;

	// NOTE: copy input samples to WASM	
	for(let channelIdx = 0; channelIdx < inputChannelCount; ++channelIdx) {
	    const inputSamplesOffset = this.wasmInstance.exports.getInputSamplesOffset(channelIdx);
	    const inputSamples = new Float32Array(this.sharedMemory.buffer, inputSamplesOffset, inputSampleCount);
	    for(let sampleIdx = 0; sampleIdx < inputSampleCount; ++sampleIdx) {
		inputSamples[sampleIdx] = input[channelIdx][sampleIdx];
	    }
	}

	// NOTE: run the audio process
	this.wasmInstance.exports.audioProcess(sampleRate, inputChannelCount, inputSampleCount, outputChannelCount, outputSampleCount);

	// NOTE: read output samples from WASM
	for(let channelIdx = 0; channelIdx < outputChannelCount; ++channelIdx) {

	    const outputSamplesOffset = this.wasmInstance.exports.getOutputSamplesOffset(channelIdx);
	    const outputSamples = new Float32Array(this.sharedMemory.buffer, outputSamplesOffset, outputSampleCount);

	    for(let sampleIdx = 0; sampleIdx < outputSampleCount; ++sampleIdx) {
		output[channelIdx][sampleIdx] = outputSamples[sampleIdx];
	    }
	}

	return true;
    }
}

registerProcessor("granade-processor", GranadeProcessor);
