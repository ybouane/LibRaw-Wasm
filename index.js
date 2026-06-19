export default class LibRaw {
	constructor() {
		this.worker = new Worker(new URL('./worker.js', import.meta.url), {type:"module"});
		this.pending = new Map();      // id -> { resolve, reject }
		this.nextId = 0;
		this.tail = Promise.resolve(); // serializes calls on this (stateful) instance
		this.disposed = false;
		this.worker.onmessage = ({data}) => {
			let slot = this.pending.get(data?.id);
			if(!slot) {
				return; // unknown or already-settled reply: ignore, never hang
			}
			this.pending.delete(data.id);
			if(data?.error) {
				slot.reject(new Error(data.error));
			} else {
				slot.resolve(data?.out);
			}
		};
	}

	/**
	 * Dispose of the worker. Rejects any in-flight calls; the instance is unusable afterwards.
	 */
	dispose() {
		this.disposed = true;
		this.worker.terminate();
		for(let {reject} of this.pending.values()) {
			reject(new Error('LibRaw disposed'));
		}
		this.pending.clear();
	}

	runFn(fn, ...args) {
		let exec = () => new Promise((resolve, reject)=>{
			if(this.disposed) { // disposed while queued behind an earlier call
				reject(new Error('LibRaw disposed'));
				return;
			}
			let id = this.nextId++;
			this.pending.set(id, {resolve, reject});
			this.worker.postMessage({id, fn, args}, args.map(a=>{
				if([ArrayBuffer, Uint8Array, Int8Array, Uint16Array, Int16Array, Uint32Array, Int32Array, Float32Array, Float64Array].some(b=>a instanceof b)) { // Transfer buffer
					return a.buffer;
				}
			}).filter(a=>a));
		});
		// Only one call in flight per instance; a rejection must not break the chain.
		let result = this.tail.then(exec, exec);
		this.tail = result.then(()=>{}, ()=>{});
		return result;
	}
	/**
	 * Open/parse the RAW data with optional settings
	 */
	async open(buffer, settings) {
		return await this.runFn('open', buffer, settings);
	}

	/**
	 * Retrieve metadata
	 */
	async metadata(fullOutput) {
		let metadata = await this.runFn('metadata', !!fullOutput);
		// Example: convert numeric thumb_format to a string
		if (metadata?.hasOwnProperty('thumb_format')) {
			metadata.thumb_format = [
				'unknown',
				'jpeg',
				'bitmap',
				'bitmap16',
				'layer',
				'rollei',
				'h265'
			][metadata.thumb_format] || 'unknown';
		}
		// Trim desc if present
		if (metadata?.hasOwnProperty('desc')) {
			metadata.desc = String(metadata.desc).trim();
		}
		if (metadata?.hasOwnProperty('timestamp')) {
			// LibRaw's timestamp is a time_t in epoch seconds; JS Date expects milliseconds.
			metadata.timestamp = new Date(metadata.timestamp * 1000);
		}
		return metadata;
	}

	/**
	 * Retrieve processed image data (synchronously from the perspective of C++,
	 * but we've already awaited the module & instance.)
	 */
	async imageData() {
		return await this.runFn('imageData');
	}

	/**
	 * Retrieve the raw, undebayered sensor data (16-bit mosaic, no demosaicing).
	 */
	async rawImageData() {
		return await this.runFn('rawImageData');
	}

	/**
     * Retrieve the embedded JPEG preview (Fast extraction)
     */
    async thumbnailData() {
        return await this.runFn('thumbnailData');
    }
}