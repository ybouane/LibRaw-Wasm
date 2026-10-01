// Unit test: a worker that fails to load must not hang the client.
//
// A browser reports a worker script that cannot load (blocked, missing, wrong
// MIME type), or an uncaught error in it, as an `error` event on the Worker;
// for the multi-threaded build a pool worker that cannot start ends up there
// too. Such a worker never answers, so every in-flight call, and every later
// one, has to reject instead of waiting forever.
//
// Drives the REAL client (../index.js) against a mock Worker that never
// answers and fires the events on demand. No test framework — run with
// `node test/worker-error.test.mjs`.

let last;
class MockWorker {
	constructor(url) {
		this.url = String(url);
		this.onmessage = null;
		this.onerror = null;
		this.onmessageerror = null;
		this.posted = [];
		last = this;
	}
	postMessage(msg) { this.posted.push(msg); } // never answers
	terminate() {}
}
globalThis.Worker = MockWorker;

const {default: LibRaw} = await import('../index.js');

const assert = (cond, msg) => { if (!cond) throw new Error('ASSERT: ' + msg); };
const tick = () => new Promise((resolve) => setTimeout(resolve, 0));
const withTimeout = (p, ms, label) => Promise.race([
	p, new Promise((_, rej) => setTimeout(() => rej(new Error(`TIMEOUT (hang) on ${label}`)), ms)),
]);
// Resolves with the rejection reason; fails if the promise resolves or hangs.
const rejection = async (p, label) => {
	try { await withTimeout(p, 1000, label); } catch (e) { return e; }
	throw new Error(`ASSERT: ${label} resolved`);
};

let failures = 0;
const test = async (name, fn) => {
	try { await fn(); console.log(`  ok   ${name}`); }
	catch (e) { failures++; console.log(`  FAIL ${name}\n         ${e.message}`); }
};

await test('error event: the in-flight call and the calls queued behind it reject', async () => {
	const raw = new LibRaw();
	const open = raw.open(new Uint8Array(4), {});
	const img = raw.imageData();
	await tick();
	assert(last.posted.length === 1, `one call in flight (got ${last.posted.length})`);
	last.onerror({message: 'Failed to load worker.js', filename: 'worker.js', lineno: 1});
	const e1 = await rejection(open, 'open');
	assert(/LibRaw worker failed: Failed to load worker\.js/.test(e1.message), `error message (got ${e1.message})`);
	const e2 = await rejection(img, 'imageData');
	assert(e2 === e1, 'the queued call rejects with the same error');
	assert(last.posted.length === 1, `the queued call was never posted (got ${last.posted.length})`);
});

await test('every later call rejects with the same error', async () => {
	const raw = new LibRaw();
	await tick();
	last.onerror({message: 'boom'});
	const e1 = await rejection(raw.metadata(), 'metadata');
	const e2 = await rejection(raw.imageData(), 'imageData');
	assert(e1 === e2 && /boom/.test(e1.message), `same error (got ${e1.message} / ${e2.message})`);
});

await test('multi-threaded build: a failed configure rejects the first real call', async () => {
	const raw = new LibRaw({threads: 4});
	const info = raw.runtimeInfo();
	await tick();
	assert(last.url.endsWith('/worker-threaded.js') && last.posted[0]?.fn === 'configure', 'configure is in flight');
	last.onerror({message: 'Failed to load libraw-threaded.js'});
	const e = await rejection(info, 'runtimeInfo');
	assert(/libraw-threaded\.js/.test(e.message), `error message (got ${e.message})`);
});

await test('an error event without a message still rejects', async () => {
	const raw = new LibRaw();
	const p = raw.metadata();
	await tick();
	last.onerror({});
	const e = await rejection(p, 'metadata');
	assert(/LibRaw worker failed: /.test(e.message), `error message (got ${e.message})`);
});

await test('messageerror event rejects too', async () => {
	const raw = new LibRaw();
	const p = raw.metadata();
	await tick();
	last.onmessageerror({});
	const e = await rejection(p, 'metadata');
	assert(/could not be deserialized/.test(e.message), `error message (got ${e.message})`);
});

await test('dispose() after a failure is harmless', async () => {
	const raw = new LibRaw();
	const p = raw.metadata();
	await tick();
	last.onerror({message: 'boom'});
	await rejection(p, 'metadata');
	raw.dispose();
	const e = await rejection(raw.metadata(), 'metadata after dispose');
	assert(e instanceof Error, 'rejects');
});

console.log(failures === 0 ? '\nAll worker-error tests passed.' : `\n${failures} test(s) failed.`);
process.exit(failures === 0 ? 0 : 1);
