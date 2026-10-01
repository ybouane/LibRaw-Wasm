<sub>*Follow me on X [@ybouane](https://x.com/ybouane) — I'm building in public.*</sub>
# LibRaw-Wasm
A WebAssembly build of LibRaw, powered by Emscripten and leveraging Web Workers. This lets you decode and process RAW image files directly in the browser or in a Node.js environment supporting WebAssembly. With LibRaw-Wasm, you can extract metadata and obtain decoded image data from formats such as CR2, NEF, ARW, DNG, and more.

This package provides an asynchronous API for opening RAW images and processing them using the same robust codebase behind LibRaw.

LibRaw-Wasm's processing is done in a Web Worker to avoid blocking the main UI thread.


# Install
```bash
npm install libraw-wasm
```

# Basic usage
```javascript
import LibRaw from 'libraw-wasm';

const output = document.getElementById('output');
// Instantiate LibRaw
const raw = new LibRaw();
// Open (decode) the RAW file
await raw.open(new Uint8Array(fileBuffer), { /* settings */ });

// Fetch metadata
const meta = await raw.metadata(/* fullOutput=false */);
console.log('Metadata:', meta);
output.innerText = JSON.stringify(meta, null, 4);

// Fetch the decoded image data (RGB pixels).
// imageData() rejects if decoding fails (e.g. a compression format this build
// can't decode), so wrap it in try/catch when handling untrusted files.
try {
	const imageData = await raw.imageData();
	console.log('Image data:', imageData);
	console.log('Image data length:', imageData.data.length);
} catch (err) {
	console.error('Failed to decode image:', err);
}

// Fetch the raw, undebayered sensor data (16-bit mosaic, no demosaicing)
const rawImageData = await raw.rawImageData();
console.log('Raw sensor data:', rawImageData); // { raw_width, raw_height, width, height, top_margin, left_margin, data: Uint16Array }

```

# Settings
```javascript
{
	bright: 1.0,			// -b <float> : brightness
	threshold: 0.0,			// -n <float> : wavelet denoise threshold
	autoBrightThr: 0.01,	// portion of clipped pixels for auto-brightening
	adjustMaximumThr: 0.75,	// auto-adjust max if channel overflow above threshold
	expShift: 1.0,			// exposure shift in linear scale (requires expCorrec=1)
	expPreser: 0.0,			// preserve highlights when expShift>1 (0..1)

	halfSize: false,		// -h  : output at 1/2 size
	fourColorRgb: false,	// -f  : separate interpolation for two green channels
	highlight: 0,			// -H  : highlight mode (0..9)
	useAutoWb: false,		// -a  : auto white balance
	useCameraWb: false,		// -w  : camera's recorded WB
	useCameraMatrix: 1,		// +M/-M : color profile usage (0=off,1=on if WB,3=always)
	outputColor: 1,			// -o  : output colorspace (0..8) (0=raw,1=sRGB,2=Adobe, etc.)
	outputBps: 8,			// -4  : 8 or 16 bits per sample
	outputTiff: false,		// -T  : output TIFF if true, else PPM
	outputFlags: 0,			// bitfield for custom output flags
	userFlip: -1,			// -t  : flip/rotate (0..7, default=-1 means use RAW value)
	userQual: 3,			// -q  : interpolation quality (0..12)
	userBlack: -1,			// -k  : user black level
	userCblack: [-1, -1, -1, -1], // per-channel black offsets
	userSat: 0,				// -S  : saturation level
	medPasses: 0,			// -m  : median filter passes
	noAutoBright: false,	// -W  : don't apply auto brightness
	useFujiRotate: -1,		// -j  : -1=use, 0=off, 1=on, for Fuji sensor rotation
	greenMatching: false,	// fix green channel imbalance (not a dcraw key)
	dcbIterations: -1,		// additional DCB passes (-1=off)
	dcbEnhanceFl: false,	// enhance color fidelity in DCB
	fbddNoiserd: 0,			// 0=off,1=light,2=full FBDD denoise
	expCorrec: false,		// enable exposure correction (then expShift, expPreser apply)
	noAutoScale: false,		// skip scale_colors (affects WB)
	noInterpolation: false,	// skip demosaic entirely (outputs raw mosaic)

	greybox: null,			// -A x y w h : rectangle (x,y,width,height) for WB calc
	cropbox: null,			// Cropping rectangle (left, top, w, h) applied before rotation
	aber: null,				// -C (red multiplier = aber[0], blue multiplier = aber[2])
	gamm: null,				// -g power toe_slope (1/power -> gamm[0], gamm[1] -> slope)
	userMul: null,			// -r mul0 mul1 mul2 mul3 : user WB multipliers (r, g, b, g2)

	outputProfile: null,	// -o <filename> : output ICC profile (if compiled w/ LCMS)
	cameraProfile: null,	// -p <filename> or 'embed' : camera ICC profile
	badPixels: null,		// -P <file> : file with bad pixels map
	darkFrame: null,		// -K <file> : file with dark frame (16-bit PGM)
}
```


# Multi-threaded decoding
LibRaw's default demosaic (AHD) and several of its decoders can run on more than one thread. The package ships a second, multi-threaded build for this; pass `threads` to use it:

```javascript
const raw = new LibRaw({ threads: 4 }); // or 'auto': min(navigator.hardwareConcurrency, 8)
await raw.open(new Uint8Array(fileBuffer), settings);
const imageData = await raw.imageData();
console.log(await raw.runtimeInfo()); // { threaded: true, crossOriginIsolated: true, poolSize: 3, threads: 4 }
raw.dispose(); // also stops the instance's thread pool
```

- **Cross-origin isolation.** The decoder shares its memory with a pool of nested workers, which browsers only allow on [cross-origin isolated](https://developer.mozilla.org/en-US/docs/Web/API/Window/crossOriginIsolated) pages (`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`) that expose `SharedArrayBuffer` (some webviews, such as WKWebView on macOS, report isolation without it). Elsewhere the same build decodes on one thread.
- **Thread count.** `'auto'` means `min(navigator.hardwareConcurrency, 8)`; a finite number is floored and clamped to `1…LibRaw.features.maxThreads` (16); `null` or `undefined` keep the single-threaded build; anything else throws a `TypeError`. Each instance starts its pool of `threads - 1` workers before its first call completes, so size it to what you need.
- **Memory.** The multi-threaded build can grow to 4 GB (the single-threaded build: 2 GB), which should be enough for files of about 100 MP (not tested yet).
- **Same pixels.** Both builds decode a file to exactly the same image, whatever the thread count (see below), with one exception: with wavelet denoising (`threshold` > 0), LibRaw's OpenMP version of `wavelet_denoise()` does part of its arithmetic in double instead of float, so the multi-threaded build differs there from the single-threaded one by a few pixels (it is still the same at every thread count).
- **Feature detection.** `LibRaw.features?.threads` is `true` in versions that ship the multi-threaded build (older versions ignore the constructor argument).
- **If the worker cannot load.** When the worker, or one of its pool workers, fails to load (a blocked or missing script, a wrong MIME type), every call on the instance rejects instead of hanging. `await raw.runtimeInfo()` is the check to make up front; to fall back:
  ```javascript
  let raw = new LibRaw({ threads: 4 });
  try { await raw.runtimeInfo(); } catch (err) { raw.dispose(); raw = new LibRaw(); }
  ```
- **Bundlers.** The worker and its pool workers are created with `new Worker(new URL(..., import.meta.url))`, which Vite and webpack follow. Both builds are referenced statically, so a bundle contains the multi-threaded worker, its glue and its ~1.6 MB wasm even when `threads` is never passed; they are only fetched when it is. The two builds' pool workers have different names (`em-pthread` and `em-pthread-mt`), so webpack keeps them in separate chunks.

# Deterministic output
Both builds compile LibRaw with IEEE-strict floating point, so for a given LibRaw version and settings LibRaw's own processing gives the same pixels whatever the Emscripten version, the machine that built the package, or the thread count (checked: the wasm build matches native arm64 and x86_64 clang builds of the same sources).
- No `-ffast-math` (it lets the compiler reorder arithmetic and assume there are no NaNs or infinities) and no fused multiply-add contraction (`-ffp-contract=off`).
- LibRaw's calls to `pow`, `powf`, `exp`, `log`, `logf` and `cos` (its gamma curve, the CIELab table AHD uses, and a few others) go to [unmodified copies of musl's implementations](portable-math/README.md) instead of the toolchain's libm, so neither a libm update nor the compiler's folding or rewriting of those calls changes the result.
- LibRaw's parallel regions write disjoint pixels, so the thread count does not change the result — except in the X-Trans (Fuji) demosaic, the X-Trans half-size path and the DHT demosaic (`userQual: 11`), where threads read or overwrite pixels other threads are writing. For those (any colour filter array other than Bayer, or DHT), the multi-threaded build develops the image on one thread; its decoders still run in parallel.
- Two optional parts still come from the toolchain: colour management with `cameraProfile`/`outputProfile` (LCMS2 uses Emscripten's libm) and lossy DNG decoding (Emscripten's libjpeg port). With wavelet denoising (`threshold` > 0) the two builds differ from each other, see above.
- Deflate-compressed DNGs are not supported (`--disable-zlib`), as in earlier versions. This is now pinned, so that a build does not depend on what the build machine's Emscripten cache contains.

# Additional Notes
- **Performance:** Decoding large RAW files in the browser can be CPU-intensive. See [Multi-threaded decoding](#multi-threaded-decoding).
- **Memory:** WebAssembly modules can allocate a significant amount of memory. Check your environment’s limits if you work with very large files.

## Local development
 - If you're making changes in the CPP wrapper, launch `compileLibraw.sh` (or `npm run compile`). It builds the LCMS, LibRaw (single-threaded and OpenMP) and portable-math static libs once into `libs/`/`includes/` and reuses them on subsequent runs; set `FORCE_LIBS=1` to rebuild them (e.g. after changing pinned versions or `portable-math/`).
 - If you're launching it on MacOS, make sure that emscripten is installed (e.g. `brew install emscripten`) + build dependencies are insalled (e.g. `brew install autoconf automake libtool`). The pinned toolchain is Emscripten 6.0.4 (the multi-threaded build needs the wasm32 OpenMP runtime that Emscripten ships since 6.0.3).
 - Tests: `npm test` runs the fast unit tests (worker reply routing, the `threads` option); `npm run test:integration` decodes `example-sony.ARW` in headless Chromium, including the multi-threaded build on an isolated and a non-isolated page (run `npx playwright install chromium` first).

## CI/CD
 - **PRs** (`ci.yml`): the wasm is built from source and the full test suite runs on every pull request — so you do **not** need to build or commit any binaries (`dist/`, `libraw.wasm`, …). CI regenerates them.
 - **main** (`build-artifacts.yml`): when a build-affecting file changes on `main`, CI rebuilds the wasm and commits the regenerated artifacts back, keeping the checked-in binaries authoritative.
 - **Releases** (`release.yml`): pushing a `v*` tag builds from source, tests, and publishes to npm (with provenance, via OIDC trusted publishing) plus a GitHub Release. Cut one with `npm version <patch|minor|major> && git push --follow-tags`.
