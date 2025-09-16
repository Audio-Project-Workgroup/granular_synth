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
    
    function platformSin(val) {
	return Math.sin(val);
    }

    function platformCos(val) {
	return Math.cos(val);
    }

    const importObject = {
	env: {
	    memory: memory,
	    platformLog,
	    platformSin,
	    platformCos,
	}
    };
    return importObject;
}

export { makeImportObject };
