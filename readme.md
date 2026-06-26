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


# Additional Notes
- **Performance:** Decoding large RAW files in the browser can be CPU-intensive.
- **Memory:** WebAssembly modules can allocate a significant amount of memory. Check your environment’s limits if you work with very large files.

## Local development
 - If you're making changes in the CPP wrapper, launch `compileLibraw.sh` (or `npm run compile`). It builds the LCMS + LibRaw static libs once into `libs/`/`includes/` and reuses them on subsequent runs; set `FORCE_LIBS=1` to rebuild them (e.g. after changing pinned versions).
 - If you're launching it on MacOS, make sure that emscripten is installed (e.g. `brew install emscripten`) + build dependencies are insalled (e.g. `brew install autoconf automake libtool`). The pinned toolchain is Emscripten 5.0.7.
 - Tests: `npm test` runs the fast worker reply-routing unit test; `npm run test:integration` decodes `example-sony.ARW` in headless Chromium (run `npx playwright install chromium` first).

## CI/CD
 - **PRs** (`ci.yml`): the wasm is built from source and the full test suite runs on every pull request — so you do **not** need to build or commit any binaries (`dist/`, `libraw.wasm`, …). CI regenerates them.
 - **main** (`build-artifacts.yml`): when a build-affecting file changes on `main`, CI rebuilds the wasm and commits the regenerated artifacts back, keeping the checked-in binaries authoritative.
 - **Releases** (`release.yml`): pushing a `v*` tag builds from source, tests, and publishes to npm (with provenance, via OIDC trusted publishing) plus a GitHub Release. Cut one with `npm version <patch|minor|major> && git push --follow-tags`.
