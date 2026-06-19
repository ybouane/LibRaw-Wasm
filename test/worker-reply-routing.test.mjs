// Regression test for the worker reply-routing bug.
//
// Before the fix, the client kept a single pending-reply slot, so a second
// concurrent call overwrote the first call's reply handler: the metadata reply
// resolved the imageData promise (wrong payload) and the metadata promise never
// settled -> Promise.all hung forever.
//
// This test drives the REAL client (../index.js) against a mock Worker that
// mirrors worker.js semantics: messages processed serially in FIFO order, with
// `metadata` returning before `imageData` (the realistic ordering that triggered
// the hang). No test framework — run with `node test/worker-reply-routing.test.mjs`.

// Mock Worker installed as a global before any `new LibRaw()`.
class MockWorker {
	constructor() {
		this._onmessage = null;
		this._queue = Promise.resolve(); // serial, like a real Worker event loop
		this._terminated = false;
	}
	set onmessage(fn) { this._onmessage = fn; }
	postMessage(msg) {
		const delay = msg.fn === 'metadata' ? 10 : 50; // metadata replies first
		this._queue = this._queue
			.then(() => new Promise(r => setTimeout(r, delay)))
			.then(() => {
				if (this._terminated) return;
				// Echo the id back, just like worker.js does.
				this._onmessage({ data: { id: msg.id, out: { fn: msg.fn, payload: `${msg.fn}-result` } } });
			});
	}
	terminate() { this._terminated = true; }
}
globalThis.Worker = MockWorker;

const { default: LibRaw } = await import('../index.js');

const withTimeout = (p, ms, label) => Promise.race([
	p,
	new Promise((_, rej) => setTimeout(() => rej(new Error(`TIMEOUT (hang) on ${label}`)), ms)),
]);

const assert = (cond, msg) => { if (!cond) throw new Error('ASSERT: ' + msg); };

let failures = 0;
const test = async (name, fn) => {
	try { await fn(); console.log(`  ok   ${name}`); }
	catch (e) { failures++; console.log(`  FAIL ${name}\n         ${e.message}`); }
};

// 1. The exact original trigger: concurrent Promise.all must resolve, with each
//    promise receiving its OWN payload (no cross-wiring).
await test('Promise.all([metadata, imageData]) resolves with correct payloads', async () => {
	const raw = new LibRaw();
	const [meta, img] = await withTimeout(
		Promise.all([raw.metadata(), raw.imageData()]), 1000, 'Promise.all');
	assert(meta?.payload === 'metadata-result', `metadata got ${JSON.stringify(meta)}`);
	assert(img?.payload === 'imageData-result', `imageData got ${JSON.stringify(img)}`);
});

// 2. Sequential awaits still work.
await test('sequential metadata() then imageData() resolves correctly', async () => {
	const raw = new LibRaw();
	const meta = await withTimeout(raw.metadata(), 1000, 'metadata');
	const img = await withTimeout(raw.imageData(), 1000, 'imageData');
	assert(meta?.payload === 'metadata-result', 'metadata payload');
	assert(img?.payload === 'imageData-result', 'imageData payload');
});

// 3. Many concurrent calls each get their own reply.
await test('N concurrent calls each resolve with their own reply', async () => {
	const raw = new LibRaw();
	const fns = ['metadata', 'imageData', 'rawImageData', 'thumbnailData'];
	const results = await withTimeout(
		Promise.all(fns.map(fn => raw.runFn(fn))), 2000, 'N concurrent');
	results.forEach((r, i) => assert(r?.payload === `${fns[i]}-result`,
		`call ${fns[i]} got ${JSON.stringify(r)}`));
});

// 4. dispose() rejects in-flight calls instead of leaving them pending forever.
await test('dispose() rejects in-flight calls', async () => {
	const raw = new LibRaw();
	const p = raw.metadata();
	raw.dispose();
	let rejected = false;
	await p.catch(() => { rejected = true; });
	assert(rejected, 'in-flight call should reject after dispose()');
});

console.log(failures === 0 ? '\nAll reply-routing tests passed.' : `\n${failures} test(s) failed.`);
process.exit(failures === 0 ? 0 : 1);
