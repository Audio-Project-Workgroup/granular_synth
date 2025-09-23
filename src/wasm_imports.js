function makeImportObject(memory) {

    function cstrGetLen(ptr) {
	const mem = new Uint8Array(memory.buffer);
	let len = 0;
	while(mem[ptr] != 0) {
	    ++len;
	    ++ptr;
	}
	return len;
    }

    function cstrFromPtr(ptr) {
	const len = cstrGetLen(ptr);
	const bytes = new Uint8Array(memory.buffer, ptr, len);
	const tempBuffer = new ArrayBuffer(len);
	const tempView = new Uint8Array(tempBuffer);
	tempView.set(bytes);
	return new TextDecoder().decode(tempView);
    }

    function platformLog(msgPtr) {	
	const msg = cstrFromPtr(msgPtr);
	console.log(msg);
    }

    function platformRand(min, max) {
	const rand01 = Math.random();
	const result = (max - min) * rand01 + min;
	return result;
    }

    function platformAbs(val) {
	return Math.abs(val);
    }

    function platformSqrt(val) {
	return Math.sqrt(val);
    }

    function platformSin(val) {
	return Math.sin(val);
    }

    function platformCos(val) {
	return Math.cos(val);
    }

    function platformPow(base, exp) {
	return Math.pow(base, exp);
    }

    const importObject = {
	env: {
	    memory: memory,
	    platformLog,
	    platformRand,
	    platformAbs,
	    platformSqrt,
	    platformSin,
	    platformCos,
	    platformPow,
	}
    };
    return importObject;
}

export { makeImportObject };
