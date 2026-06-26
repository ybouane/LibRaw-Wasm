// Integration test: drive the real built module (dist/) in a headless browser
// against the example Sony RAW, asserting decode output and the concurrency /
// instance-reuse / dispose invariants.
//
// The module is built for `web,worker` and uses pthreads + SharedArrayBuffer, so
// it cannot run in Node — we serve dist/ over HTTP with the COOP/COEP headers
// cross-origin isolation requires, and drive it with Playwright's Chromium.
//
//   npm run test:integration
//
// CI installs the browser with `npx playwright install --with-deps chromium`.
// Set PW_CHANNEL=chrome to use a locally installed Google Chrome instead.

import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize, dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const FIXTURE = 'example-sony.ARW';
const MIME = {
	'.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm',
	'.html': 'text/html', '.json': 'application/json', '.map': 'application/json',
	'.arw': 'application/octet-stream', '.dng': 'application/octet-stream',
};

const PAGE = `<!doctype html><meta charset=utf-8><title>decode</title>
<script type="module">
import LibRaw from './dist/index.js';
const withTimeout = (p, ms, label) => Promise.race([
	p, new Promise((_, r) => setTimeout(() => r(new Error('TIMEOUT ' + label)), ms)),
]);
(async () => {
	try {
		const buf = await (await fetch('./${FIXTURE}')).arrayBuffer();
		// open() transfers (detaches) the input, so make independent copies up front.
		const bytes = () => new Uint8Array(buf.slice(0));

		const raw = new LibRaw();
		await raw.open(bytes(), { useCameraWb: true });
		const meta = await raw.metadata(true);
		const img = await raw.imageData();
		const rawImg = await raw.rawImageData();
		const thumb = await raw.thumbnailData();

		// Concurrency: the exact Promise.all scenario that used to hang.
		const reuse = new LibRaw();
		await reuse.open(bytes(), { useCameraWb: true, halfSize: true });
		const [cMeta, cImg] = await withTimeout(
			Promise.all([reuse.metadata(), reuse.imageData()]), 20000, 'Promise.all');

		// Instance reuse: a 2nd open() on the same instance must reflect new settings.
		await reuse.open(bytes(), { useCameraWb: true, halfSize: false });
		const reImg = await reuse.imageData();

		// dispose() must reject in-flight calls instead of hanging.
		const d = new LibRaw();
		await d.open(bytes(), {});
		const inflight = d.metadata();
		d.dispose();
		let disposedRejected = false;
		await inflight.catch(() => { disposedRejected = true; });

		// Lossy DNG (Adobe lossy / baseline-JPEG, compression 34892, LinearRaw) — issue #27.
		// This path needs libjpeg linked in: with the old USE_JPEG-off build lossy_dng_load_raw()
		// was an empty stub and imageData() resolved with nothing. The fixture encodes R ramping
		// left->right and G ramping top->bottom, so we assert the gradient survived the JPEG
		// round-trip — proving libjpeg actually decoded the tile, not just "non-empty".
		const dngBuf = await (await fetch('./test/integration/lossy.dng')).arrayBuffer();
		const dngRaw = new LibRaw();
		await dngRaw.open(new Uint8Array(dngBuf.slice(0)), { useCameraWb: true, outputBps: 8 });
		let dngImg = null, dngErr = null;
		try { dngImg = await dngRaw.imageData(); } catch (e) { dngErr = String(e && e.message || e); }
		let dngStats = null;
		if (dngImg && dngImg.data && dngImg.width && dngImg.height) {
			const W = dngImg.width, H = dngImg.height, C = dngImg.colors || 3, d = dngImg.data;
			const tx = Math.floor(W / 3), ty = Math.floor(H / 3);
			let rL = 0, rR = 0, gT = 0, gB = 0, nz = 0;
			for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
				const i = (y * W + x) * C;
				if (d[i] || d[i + 1] || d[i + 2]) nz++;
				if (x < tx) rL += d[i]; else if (x >= W - tx) rR += d[i];
				if (y < ty) gT += d[i + 1]; else if (y >= H - ty) gB += d[i + 1];
			}
			const nx = tx * H, ny = ty * W;
			dngStats = { rLeft: rL / nx, rRight: rR / nx, gTop: gT / ny, gBottom: gB / ny, nonzero: nz };
		}
		dngRaw.dispose();

		window.__RESULT = {
			ok: true,
			model: meta?.camera_model,
			dngW: dngImg?.width, dngH: dngImg?.height, dngColors: dngImg?.colors,
			dngLen: dngImg?.data?.length, dngCtor: dngImg?.data?.constructor?.name,
			dngErr, dngStats,
			tsYear: meta?.timestamp instanceof Date ? meta.timestamp.getUTCFullYear() : null,
			tsIso: meta?.timestamp instanceof Date ? meta.timestamp.toISOString() : null,
			lens: meta?.lens?.Lens,
			imgW: img?.width, imgH: img?.height, imgLen: img?.data?.length, imgCtor: img?.data?.constructor?.name,
			rawLen: rawImg?.data?.length, rawCtor: rawImg?.data?.constructor?.name,
			thumbW: thumb?.width, thumbH: thumb?.height, thumbFmt: thumb?.format,
			concMeta: cMeta?.camera_model, concImgW: cImg?.width,
			reHalfW: cImg?.width, reFullW: reImg?.width,
			disposedRejected,
		};
	} catch (e) { window.__RESULT = { ok: false, error: String(e && e.message || e), stack: e && e.stack }; }
})();
</script>`;

const server = http.createServer(async (req, res) => {
	res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
	res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
	res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
	const url = decodeURIComponent(req.url.split('?')[0]);
	if (url === '/' || url === '/index.html') {
		res.setHeader('Content-Type', 'text/html');
		return res.end(PAGE);
	}
	try {
		const p = normalize(join(ROOT, url));
		if (!p.startsWith(ROOT)) { res.statusCode = 403; return res.end('forbidden'); }
		const data = await readFile(p);
		res.setHeader('Content-Type', MIME[extname(p).toLowerCase()] || 'application/octet-stream');
		res.end(data);
	} catch { res.statusCode = 404; res.end('not found'); }
});

await new Promise((r) => server.listen(0, r));
const port = server.address().port;

const launchOpts = { headless: true };
if (process.env.PW_CHANNEL) launchOpts.channel = process.env.PW_CHANNEL;
const browser = await chromium.launch(launchOpts);
const page = await browser.newPage();
const logs = [];
page.on('console', (m) => { if (m.type() === 'error') logs.push('[console.error] ' + m.text()); });
page.on('pageerror', (e) => logs.push('[pageerror] ' + e.message));

let r;
try {
	await page.goto(`http://localhost:${port}/`);
	await page.waitForFunction('window.__RESULT !== undefined', { timeout: 120000 });
	r = await page.evaluate('window.__RESULT');
} catch (e) {
	r = { ok: false, error: 'driver: ' + e.message };
} finally {
	await browser.close();
	server.close();
}

if (logs.length) console.log(logs.join('\n'));
console.log(JSON.stringify(r, null, 2));

// Assertions
const checks = [];
const check = (cond, msg) => checks.push({ ok: !!cond, msg });
check(r && r.ok, 'page ran without error');
if (r && r.ok) {
	check(r.model === 'ILME-FX30', `model is ILME-FX30 (got ${r.model})`);
	check(r.tsYear && r.tsYear >= 2020, `timestamp scaled to a real year (got ${r.tsIso})`);
	check(r.imgW === 6240 && r.imgH === 4168, `imageData full dims 6240x4168 (got ${r.imgW}x${r.imgH})`);
	check(r.imgCtor === 'Uint8Array', `imageData is Uint8Array (got ${r.imgCtor})`);
	check(r.rawLen === 6272 * 4168, `rawImageData full mosaic length (got ${r.rawLen})`);
	check(r.rawCtor === 'Uint16Array', `rawImageData is Uint16Array (got ${r.rawCtor})`);
	check(r.thumbW === 6192 && r.thumbH === 4128, `thumbnail dims 6192x4128 (got ${r.thumbW}x${r.thumbH})`);
	check(r.thumbFmt === 'jpeg', `thumbnail format jpeg (got ${r.thumbFmt})`);
	check(r.concMeta === 'ILME-FX30' && r.concImgW === 3120, 'concurrent Promise.all resolved with correct payloads');
	check(r.reHalfW === 3120 && r.reFullW === 6240, `instance reuse reflects new settings (half=${r.reHalfW}, full=${r.reFullW})`);
	check(r.disposedRejected, 'dispose() rejected the in-flight call');

	// Lossy DNG decode (issue #27) — fails (empty/throws) on a build without libjpeg.
	const s = r.dngStats;
	check(r.dngErr == null, `lossy DNG imageData did not throw (err=${r.dngErr})`);
	check(r.dngW === 256 && r.dngH === 168, `lossy DNG dims 256x168 (got ${r.dngW}x${r.dngH})`);
	check(r.dngLen === 256 * 168 * 3, `lossy DNG data length 129024 (got ${r.dngLen})`);
	check(r.dngCtor === 'Uint8Array', `lossy DNG data is Uint8Array (got ${r.dngCtor})`);
	check(s && s.nonzero > 0, `lossy DNG decoded non-empty pixels (nonzero=${s?.nonzero})`);
	check(s && s.rRight > s.rLeft + 20, `lossy DNG red gradient L->R (L=${s?.rLeft?.toFixed(1)} R=${s?.rRight?.toFixed(1)})`);
	check(s && s.gBottom > s.gTop + 20, `lossy DNG green gradient T->B (T=${s?.gTop?.toFixed(1)} B=${s?.gBottom?.toFixed(1)})`);
}

const failed = checks.filter((c) => !c.ok);
for (const c of checks) console.log(`  ${c.ok ? 'ok  ' : 'FAIL'} ${c.msg}`);
console.log(failed.length === 0 ? '\nAll integration checks passed.' : `\n${failed.length} integration check(s) failed.`);
process.exit(failed.length === 0 ? 0 : 1);
