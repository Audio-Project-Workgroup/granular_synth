let vsSource = `
attribute vec2 v_pattern;
attribute vec4 v_min_max;
uniform mat4 transform;
uniform mat4 rotate;

void main() {
  vec2 center = (v_min_max.zw + v_min_max.xy)*0.5;
  vec2 half_dim = (v_min_max.zw - v_min_max.xy)*0.5;
  vec2 pos = center + half_dim*v_pattern;
  gl_Position = transform * rotate * vec4(pos, 0.0, 1.0);
}
`;

let fsSource = `
void main() {
  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
`;

async function main() {
    const sharedMemory = new WebAssembly.Memory({
	initial: 2,
	maximum: 2,
	shared: false,
    });
    const wasm = await WebAssembly.instantiateStreaming(fetch("./build/wasm_glue.wasm"), {
	env: {
	    memory: sharedMemory,
	}	
    });
    //console.log(wasm.instance.exports.fmadd(4, 5, 6));
    
    const canvas = document.querySelector("#gl-canvas");
    const gl = canvas.getContext("webgl2");
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);

    const audioCtx = new AudioContext();    
    await audioCtx.audioWorklet.addModule("./src/granade_audio.js");
    const granadeNode = new AudioWorkletNode(audioCtx, "granade-processor");
    granadeNode.connect(audioCtx.destination);

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
    const transformPosition = gl.getUniformLocation(shaderProgram, "transform");
    const rotatePosition = gl.getUniformLocation(shaderProgram, "rotate");

    // render loop
    function render(now) {
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.clearDepth(1.0);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

	// init buffers
	const vertexBuffer = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
	const rectMinX1 = 0.1*canvas.width;
	const rectMinY1 = 0.1*canvas.height;
	const rectMaxX1 = 0.4*canvas.width;
	const rectMaxY1 = 0.4*canvas.height;
	const rectMinX2 = 0.6*canvas.width;
	const rectMinY2 = 0.6*canvas.height;
	const rectMaxX2 = 0.9*canvas.width;
	const rectMaxY2 = 0.9*canvas.height;    
	const positions = [
	    1.0, 1.0,
	    -1.0, 1.0,
	    1.0, -1.0,
	    -1.0, -1.0,

	    // rectMinX1, rectMinY1, rectMaxX1, rectMaxY1,
	    // rectMinX2, rectMinY2, rectMaxX2, rectMaxY2,
	];
	gl.bufferData(gl.ARRAY_BUFFER, 1024*16, gl.STATIC_DRAW);
	gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Float32Array(positions));
	
	const quadOffset = wasm.instance.exports.beginDrawQuads(canvas.width, canvas.height);
	const quadCount = wasm.instance.exports.endDrawQuads();
	//console.log(quadOffset);
	//console.log(quadCount);
	const quadData = new Float32Array(sharedMemory.buffer, quadOffset, 4*quadCount);
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
	const vMinMaxStride = 0;
	const vMinMaxOffset = 32;
	gl.enableVertexAttribArray(vMinMaxPosition);
	gl.vertexAttribDivisor(vMinMaxPosition, 1);
	gl.vertexAttribPointer(vMinMaxPosition, vMinMaxNumComponents, vMinMaxType, vMinMaxNormalize, vMinMaxStride, vMinMaxOffset);

	const a = 2.0 / canvas.width;
	const b = 2.0 / canvas.height;
	const transformMatrix = [
	    a,    0.0, 0.0, 0.0,
	    0.0,  b,   0.0, 0.0,
	    0.0,  0.0, 1.0, 0.0,
	    -1.0, -1.0, 0.0, 1.0,
	];
	gl.uniformMatrix4fv(transformPosition, false, new Float32Array(transformMatrix));

	const angularVelocity = 2; // rad/s
	const angle = now * 0.001 * angularVelocity;
	const cosA = Math.cos(angle);
	const sinA = Math.sin(angle);
	const centerX = 0.5 * canvas.width;
	const centerY = 0.5 * canvas.height;
	const rotateMatrix = [
	    cosA, sinA, 0.0, 0.0,
	    -sinA, cosA, 0.0, 0.0,
	    0.0, 0.0, 1.0, 0.0,
	    centerX*(1.0 - cosA) + centerY*sinA, centerY*(1.0 - cosA) - centerX*sinA, 0.0, 1.0,
	];
	gl.uniformMatrix4fv(rotatePosition, false, new Float32Array(rotateMatrix));

	// draw
	gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, quadCount);

	// loop
	requestAnimationFrame(render);
    }
    requestAnimationFrame(render);
}

main();
