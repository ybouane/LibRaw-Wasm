// Unit test for the worker side of the `threads` option (worker-core.js): how
// many threads the multi-threaded build's worker starts, from the page's
// cross-origin isolation and SharedArrayBuffer.
//
// Drives the REAL message loop against a mock Emscripten module factory that
// records the pool size it is asked for, with a mock `self` for the worker's
// global scope. No test framework — run with `node test/worker-core.test.mjs`.

const { default: serveLibRaw } = await import('../worker-core.js');

const assert = (cond, msg) => { if (!cond) throw new Error('ASSERT: ' + msg); };

let failures = 0;
const test = async (name, fn) => {
	try { await fn(); console.log(`  ok   ${name}`); }
	catch (e) { failures++; console.log(`  FAIL ${name}\n         ${e.message}`); }
};

// Serves one mock module in a mock worker scope, sends the 'configure' call
// index.js sends first, and returns the reply and the module arguments.
async function configure({threaded, crossOriginIsolated, threads}) {
	const replies = [];
	const moduleArgs = [];
	globalThis.self = {crossOriginIsolated, postMessage: (message) => replies.push(message)};
	serveLibRaw(async (args) => {
		moduleArgs.push(args);
		return {setThreadCount: (n) => n, LibRaw: class {}};
	}, {threaded});
	await self.onmessage({data: {id: 1, fn: 'configure', args: [{threads}]}});
	return {reply: replies.find((message) => message.id === 1), moduleArgs};
}

await test('isolated with SharedArrayBuffer: the requested threads, a pool of threads - 1', async () => {
	const {reply, moduleArgs} = await configure({threaded: true, crossOriginIsolated: true, threads: 4});
	assert(JSON.stringify(reply.out) === JSON.stringify({threaded: true, crossOriginIsolated: true, poolSize: 3, threads: 4}),
		`runtime info (got ${JSON.stringify(reply.out)})`);
	assert(moduleArgs[0]?.librawThreadPoolSize === 3, `pool size (got ${JSON.stringify(moduleArgs)})`);
});

await test('not isolated: one thread, no pool', async () => {
	const {reply, moduleArgs} = await configure({threaded: true, crossOriginIsolated: false, threads: 4});
	assert(reply.out.threads === 1 && reply.out.poolSize === 0 && reply.out.crossOriginIsolated === false,
		`runtime info (got ${JSON.stringify(reply.out)})`);
	assert(moduleArgs[0]?.librawThreadPoolSize === 0, `pool size (got ${JSON.stringify(moduleArgs)})`);
});

await test('isolated without SharedArrayBuffer (WKWebView): one thread, no pool', async () => {
	const saved = globalThis.SharedArrayBuffer;
	delete globalThis.SharedArrayBuffer;
	try {
		const {reply, moduleArgs} = await configure({threaded: true, crossOriginIsolated: true, threads: 4});
		assert(reply.out.threads === 1 && reply.out.poolSize === 0 && reply.out.crossOriginIsolated === true,
			`runtime info (got ${JSON.stringify(reply.out)})`);
		assert(moduleArgs[0]?.librawThreadPoolSize === 0, `pool size (got ${JSON.stringify(moduleArgs)})`);
	} finally {
		globalThis.SharedArrayBuffer = saved;
	}
});

await test('the single-threaded build ignores isolation and starts no pool', async () => {
	const {reply, moduleArgs} = await configure({threaded: false, crossOriginIsolated: true, threads: 4});
	assert(reply.out.threaded === false && reply.out.threads === 1 && reply.out.poolSize === 0,
		`runtime info (got ${JSON.stringify(reply.out)})`);
	assert(JSON.stringify(moduleArgs[0]) === '{}', `module arguments (got ${JSON.stringify(moduleArgs)})`);
});

console.log(failures === 0 ? '\nAll worker-core tests passed.' : `\n${failures} test(s) failed.`);
process.exit(failures === 0 ? 0 : 1);
