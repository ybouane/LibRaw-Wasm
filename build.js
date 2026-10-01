import { build } from 'esbuild';
import { promises as fs } from "fs";


(async () => {
	try {
		for (const glue of ['libraw.js', 'libraw-threaded.js']) {
			let librawjs = (await fs.readFile(glue)).toString();
			librawjs = librawjs.replace(/var workerOptions=([^]+?);worker=new Worker\(new URL\("([^"]+)",import.meta.url\),workerOptions\);/, `worker=new Worker(new URL("$2",import.meta.url),$1);`); // Correction to make worker options static so that it works with vite
			if (glue === 'libraw-threaded.js') {
				// Emscripten names every pthread worker "em-pthread" and tests globalThis.name
				// against it. webpack uses that name as the worker chunk's name, so with both
				// glues named alike it merges them into one chunk whose pool workers run both.
				// Give the threaded build's pool workers their own name (in both glue files the
				// name appears exactly 3 times: the Worker options and two globalThis.name checks).
				const parts = librawjs.split('"em-pthread"');
				if (parts.length !== 4) throw new Error(`expected 3 occurrences of "em-pthread" in ${glue}, found ${parts.length - 1}`);
				librawjs = parts.join('"em-pthread-mt"');
			}
			await fs.writeFile(glue, librawjs);
		}
		await build({
			entryPoints: ['index.js', 'worker.js', 'worker-threaded.js', 'libraw.js', 'libraw-threaded.js'], // Entry point of your library
			outdir: 'dist', // Output directory
			bundle: true, // Bundle all files
			minify: true, // Minify the output
			sourcemap: true, // Generate source maps
			format: 'esm', // Output format (ES Module)
		});
		await fs.copyFile('./libraw.wasm', './dist/libraw.wasm');
		await fs.copyFile('./libraw-threaded.wasm', './dist/libraw-threaded.wasm');
		await fs.copyFile('./index.d.ts', './dist/index.d.ts');
		console.log('Build successful!');
	} catch (error) {
		console.error('Build failed:', error);
		process.exit(1);
	}
})();
