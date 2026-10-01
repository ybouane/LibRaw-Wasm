// Unit test for the `threads` constructor option and feature detection.
//
// Drives the REAL client (../index.js) against a mock Worker that records the
// script it was created with and the messages posted to it, and answers every
// call like worker.js does. No test framework — run with
// `node test/threads-option.test.mjs`.

const created = [];

class MockWorker {
	constructor(url, options) {
		this.url = String(url);
		this.options = options;
		this.posted = [];
		this.threads = 1;
		this._onmessage = null;
		created.push(this);
	}
	set onmessage(fn) { this._onmessage = fn; }
	postMessage(msg) {
		this.posted.push(msg);
		const threaded = this.url.endsWith('/worker-threaded.js');
		if (msg.fn === 'configure') {
			this.threads = msg.args[0].threads;
		}
		const info = {threaded, crossOriginIsolated: true, poolSize: this.threads - 1, threads: this.threads};
		setTimeout(() => this._onmessage?.({data: {id: msg.id, out: msg.fn === 'open' ? undefined : info}}), 5);
	}
	terminate() {}
}
globalThis.Worker = MockWorker;

const {default: LibRaw} = await import('../index.js');

const assert = (cond, msg) => { if (!cond) throw new Error('ASSERT: ' + msg); };

let failures = 0;
const test = async (name, fn) => {
	try { await fn(); console.log(`  ok   ${name}`); }
	catch (e) { failures++; console.log(`  FAIL ${name}\n         ${e.message}`); }
};

const lastWorker = () => created[created.length - 1];
// Calls are posted from a promise chain, so let it run before looking.
const tick = () => new Promise((resolve) => setTimeout(resolve, 0));

await test('LibRaw.features advertises the multi-threaded build', async () => {
	assert(LibRaw.features?.threads === true, `features.threads (got ${JSON.stringify(LibRaw.features)})`);
	assert(LibRaw.features.maxThreads === 16, `features.maxThreads (got ${LibRaw.features.maxThreads})`);
	assert(Object.isFrozen(LibRaw.features), 'features is frozen');
});

await test('new LibRaw() uses the single-threaded worker and sends no configure', async () => {
	const raw = new LibRaw();
	assert(lastWorker().url.endsWith('/worker.js'), `worker script (got ${lastWorker().url})`);
	assert(lastWorker().options?.type === 'module', 'module worker');
	await raw.open(new Uint8Array(4), {});
	assert(lastWorker().posted.map(m => m.fn).join() === 'open', `calls (got ${lastWorker().posted.map(m => m.fn)})`);
	const info = await raw.runtimeInfo();
	assert(info.threaded === false && info.threads === 1, `runtimeInfo (got ${JSON.stringify(info)})`);
});

await test('new LibRaw({threads: 4}) configures the threaded worker before any other call', async () => {
	const raw = new LibRaw({threads: 4});
	const worker = lastWorker();
	assert(worker.url.endsWith('/worker-threaded.js'), `worker script (got ${worker.url})`);
	await raw.open(new Uint8Array(4), {});
	assert(worker.posted.map(m => m.fn).join() === 'configure,open', `calls (got ${worker.posted.map(m => m.fn)})`);
	assert(worker.posted[0].args[0].threads === 4, `configure threads (got ${JSON.stringify(worker.posted[0].args)})`);
	const info = await raw.runtimeInfo();
	assert(info.threaded === true && info.threads === 4 && info.poolSize === 3, `runtimeInfo (got ${JSON.stringify(info)})`);
});

await test("threads: 'auto' asks for min(hardwareConcurrency, 8)", async () => {
	new LibRaw({threads: 'auto'});
	await tick();
	const expected = Math.min(globalThis.navigator?.hardwareConcurrency || 1, 8);
	assert(lastWorker().posted[0].args[0].threads === expected,
		`auto threads (got ${lastWorker().posted[0].args[0].threads}, expected ${expected})`);
});

await test('threads is clamped to 1..maxThreads and floored', async () => {
	for (const [threads, expected] of [[0, 1], [-3, 1], [2.7, 2], [99, 16]]) {
		new LibRaw({threads});
		await tick();
		const got = lastWorker().posted[0].args[0].threads;
		assert(got === expected, `threads ${threads} -> ${got}, expected ${expected}`);
	}
});

await test('threads other than a number or "auto" is a TypeError', async () => {
	for (const threads of ['4', NaN, Infinity, {}]) {
		let error = null;
		try { new LibRaw({threads}); } catch (e) { error = e; }
		assert(error instanceof TypeError, `threads ${String(threads)} should throw a TypeError`);
	}
});

await test('threads: null or undefined keeps the single-threaded build', async () => {
	new LibRaw({threads: undefined});
	assert(lastWorker().url.endsWith('/worker.js'), 'undefined -> single-threaded');
	new LibRaw({threads: null});
	assert(lastWorker().url.endsWith('/worker.js'), 'null -> single-threaded');
});

console.log(failures === 0 ? '\nAll threads-option tests passed.' : `\n${failures} test(s) failed.`);
process.exit(failures === 0 ? 0 : 1);
