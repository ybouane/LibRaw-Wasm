// Serves one LibRaw instance to the client in index.js: a message
// {id, fn, args} calls raw[fn](...args) and is answered with {id, out} (typed
// arrays in `out` are transferred) or {id, error}.
//
// The multi-threaded build sizes its pthread pool when the module is
// instantiated, so it waits for the 'configure' call index.js sends first;
// the single-threaded build starts instantiating as soon as the worker loads.

function isTypedArray(obj) {
	return ArrayBuffer.isView(obj) && !(obj instanceof DataView);
}

export default function serveLibRaw(LibRawModule, {threaded}) {
	let ready;
	let raw;
	let info;

	const init = (options = {}) => ready ??= (async () => {
		// Other workers can only share this worker's memory when it is
		// cross-origin isolated and has SharedArrayBuffer (a webview can
		// report the first without the second: WKWebView on macOS does);
		// otherwise the same binary runs on one thread.
		const isolated = self.crossOriginIsolated === true;
		const shared = isolated && typeof SharedArrayBuffer === 'function';
		const threads = threaded && shared ? options.threads ?? 1 : 1;
		const module = await LibRawModule(threaded ? {librawThreadPoolSize: threads - 1} : {});
		info = {
			threaded,
			crossOriginIsolated: isolated,
			poolSize: threads - 1,
			threads: module.setThreadCount(threads),
		};
		raw = new module.LibRaw();
	})();

	if (!threaded) {
		init();
	}

	self.onmessage = async (event) => {
		const {id, fn, args} = event.data;
		try {
			await init(fn === 'configure' ? args[0] : undefined);
			const out = fn === 'configure' || fn === 'runtimeInfo' ? {...info} : raw[fn](...args);
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
}
