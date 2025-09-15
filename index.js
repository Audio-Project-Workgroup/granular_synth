let sharedMemory = null;

let vsSource = `
attribute vec2 v_pattern;
attribute vec4 v_min_max;
attribute vec4 v_color;
attribute float v_angle;

uniform mat4 transform;

varying lowp vec4 f_color;

void main() {
  vec2 center = (v_min_max.zw + v_min_max.xy)*0.5;
  vec2 half_dim = (v_min_max.zw - v_min_max.xy)*0.5;

  float cosa = cos(v_angle);
  float sina = sin(v_angle);
  vec2 rot_pattern = vec2(cosa*v_pattern.x - sina*v_pattern.y, sina*v_pattern.x + cosa*v_pattern.y);

  vec2 pos = center + half_dim*rot_pattern;
  gl_Position = transform * vec4(pos, 0.0, 1.0);
  f_color = v_color;
}
`;

let fsSource = `
varying lowp vec4 f_color;

void main() {
  gl_FragColor = f_color;
}
`;

function cstrGetLen(ptr) {
    const mem = new Uint8Array(sharedMemory.buffer);
    let len = 0;
    while(mem[ptr] != 0) {
	++len;
	++ptr;
    }
    return len;
}

function cstrFromPtr(ptr) {
    const len = cstrGetLen(ptr);
    const bytes = new Uint8Array(sharedMemory.buffer, ptr, len);
    const tempBuffer = new ArrayBuffer(len);
    const tempView = new Uint8Array(tempBuffer);
    tempView.set(bytes);
    return new TextDecoder().decode(tempView);
}

function platformLog(msgPtr) {
    const msg = cstrFromPtr(msgPtr);
    console.log(msg);
}

function platformSin(val) {
    return Math.sin(val);
}

function platformCos(val) {
    return Math.cos(val);
}

async function main() {

    console.log(crossOriginIsolated);    

    sharedMemory = new WebAssembly.Memory({
	initial: 2,
	maximum: 2,
	shared: true,
    });

    const importObject = {
	env: {
	    memory: sharedMemory,
	    platformLog,
	    platformSin,
	    platformCos,
	}
    };
    const wasmModule = await WebAssembly.compileStreaming(fetch("./wasm_glue.wasm"));
    const wasmInstance = await WebAssembly.instantiate(wasmModule, importObject);
    const wasm = {
	module: wasmModule,
	instance: wasmInstance,
    };
    console.log(wasm.module);
    console.log(wasm.instance);

    const wasmStateOffset = wasmInstance.exports.wasmInit(64*1024); // TODO: real heap size
    console.log(wasmStateOffset);
    const quadsOffset = wasm.instance.exports.getQuadsOffset();
    console.log(quadsOffset);

    const canvas = document.querySelector("#gl-canvas");
    const gl = canvas.getContext("webgl2");
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);

    const audioCtx = new AudioContext();
    await audioCtx.audioWorklet.addModule("./src/granade_audio.js");
    const granadeNode = new AudioWorkletNode(audioCtx, "granade-processor", {
	processorOptions: {
	    sharedMemory: sharedMemory,
	    wasmModule: wasmModule,
	}
    });
    granadeNode.connect(audioCtx.destination);
    console.log(granadeNode);

    const playButton = document.querySelector("button");
    playButton.addEventListener(
	"click",
	() => {
	    if(audioCtx.state === "running") {
		audioCtx.suspend()
	    }
	    else if(audioCtx.state === "suspended") {
		audioCtx.resume();
	    }
	},
	false
    );

    // webgl setup
    // init shaders
    const vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, vsSource);
    gl.compileShader(vertexShader);
    if(!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
	console.log(`vertex shader compilation failed: ${gl.getShaderInfoLog(vertexShader)}`);
	gl.deleteShader(vertexShader);
    }

    const fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, fsSource);
    gl.compileShader(fragmentShader);
    if(!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS)) {
	console.log(`fragment shader compilation failed: ${gl.getShaderInfoLog(fragmentShader)}`);
	gl.deleteShader(fragmentShader);
    }

    const shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);
    if(!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
	console.log(`shader compilation failed: ${gl.getProgramInfoLog(shaderProgram)}`);
    }

    // vertex attributes
    gl.useProgram(shaderProgram);
    const vPatternPosition = gl.getAttribLocation(shaderProgram, "v_pattern");
    const vMinMaxPosition = gl.getAttribLocation(shaderProgram, "v_min_max");
    const vColorPosition = gl.getAttribLocation(shaderProgram, "v_color");
    const vAnglePosition = gl.getAttribLocation(shaderProgram, "v_angle");
    const transformPosition = gl.getUniformLocation(shaderProgram, "transform");
    //const rotatePosition = gl.getUniformLocation(shaderProgram, "rotate");

    // init buffer
    const vertexBuffer = gl.createBuffer();
    const vertexBufferSize = 32*1024;
    const positions = [
	1.0,  1.0,
	-1.0,  1.0,
	1.0, -1.0,
	-1.0, -1.0,
    ];
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, vertexBufferSize, gl.STATIC_DRAW);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Float32Array(positions));

    // render loop
    function render(now) {
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.clearDepth(1.0);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

	console.log(now);

	const quadCount = wasm.instance.exports.drawQuads(canvas.width, canvas.height, now);
	const quadSize = 24;
	//console.log(`quadCount: ${quadCount}`);
	const quadFloatCount = quadSize / 4;
	const quadData = new Float32Array(sharedMemory.buffer, quadsOffset, quadFloatCount*quadCount);
	console.log(quadData);
	gl.bufferSubData(gl.ARRAY_BUFFER, 32, quadData);

	const vPatternNumComponents = 2;
	const vPatternType = gl.FLOAT;
	const vPatternNormalize = false;
	const vPatternStride = 0;
	const vPatternOffset = 0;
	gl.enableVertexAttribArray(vPatternPosition);
	gl.vertexAttribDivisor(vPatternPosition, 0);
	gl.vertexAttribPointer(vPatternPosition, vPatternNumComponents, vPatternType, vPatternNormalize, vPatternStride, vPatternOffset);

	const vMinMaxNumComponents = 4;
	const vMinMaxType = gl.FLOAT;
	const vMinMaxNormalize = false;
	const vMinMaxStride = quadSize;
	const vMinMaxOffset = 32;
	gl.enableVertexAttribArray(vMinMaxPosition);
	gl.vertexAttribDivisor(vMinMaxPosition, 1);
	gl.vertexAttribPointer(vMinMaxPosition, vMinMaxNumComponents, vMinMaxType, vMinMaxNormalize, vMinMaxStride, vMinMaxOffset);

	const vColorNumComponents = 4;
	const vColorType = gl.UNSIGNED_BYTE;
	const vColorNormalize = false;
	const vColorStride = quadSize;
	const vColorOffset = 32 + 16;
	gl.enableVertexAttribArray(vColorPosition);
	gl.vertexAttribDivisor(vColorPosition, 1);
	gl.vertexAttribPointer(vColorPosition, vColorNumComponents, vColorType, vColorNormalize, vColorStride, vColorOffset);
 
	const vAngleNumComponents = 1;
	const vAngleType = gl.FLOAT;
	const vAngleNormalize = false;
	const vAngleStride = quadSize;
	const vAngleOffset = 32 + 16 + 4;
	gl.enableVertexAttribArray(vAnglePosition);
	gl.vertexAttribDivisor(vAnglePosition, 1);
	gl.vertexAttribPointer(vAnglePosition, vAngleNumComponents, vAngleType, vAngleNormalize, vAngleStride, vAngleOffset);

	const a = 2.0 / canvas.width;
	const b = 2.0 / canvas.height;
	const transformMatrix = [
	    a,    0.0, 0.0, 0.0,
	    0.0,  b,   0.0, 0.0,
	    0.0,  0.0, 1.0, 0.0,
	    -1.0, -1.0, 0.0, 1.0,
	];
	gl.uniformMatrix4fv(transformPosition, false, new Float32Array(transformMatrix));

	// const angularVelocity = 2; // rad/s
	// const angle = now * 0.001 * angularVelocity;
	// const cosA = Math.cos(angle);
	// const sinA = Math.sin(angle);
	// const centerX = 0.5 * canvas.width;
	// const centerY = 0.5 * canvas.height;
	// const rotateMatrix = [
	//     cosA, sinA, 0.0, 0.0,
	//     -sinA, cosA, 0.0, 0.0,
	//     0.0, 0.0, 1.0, 0.0,
	//     centerX*(1.0 - cosA) + centerY*sinA, centerY*(1.0 - cosA) - centerX*sinA, 0.0, 1.0,
	// ];
	// gl.uniformMatrix4fv(rotatePosition, false, new Float32Array(rotateMatrix));

	// draw
	gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, quadCount);

	// loop
	requestAnimationFrame(render);
    }
    requestAnimationFrame(render);
}

main();
