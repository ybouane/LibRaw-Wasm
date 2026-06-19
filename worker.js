import LibRawModule from './libraw.js';

let ready;
let LibRawClass;
let raw;

async function initLibRaw() {
	ready = (async () => {
		const module = await LibRawModule();
		LibRawClass = module.LibRaw;
		raw = new LibRawClass();
	})();
}

initLibRaw();

function isTypedArray(obj) {
	return ArrayBuffer.isView(obj) && !(obj instanceof DataView);
}

self.onmessage = async (event) => {
	const {id, fn, args} = event.data;
	try {
		await ready;
		const out = raw[fn](...args);
		const transferList = [];
		for (const key in out) {
			const value = out[key];
			if (isTypedArray(value))
				transferList.push(value.buffer);
		}
		self.postMessage({id, out}, transferList);
	} catch (err) {
		self.postMessage({id, error: err.message});
	}
};