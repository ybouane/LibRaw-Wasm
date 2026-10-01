import LibRawModule from './libraw-threaded.js';
import serveLibRaw from './worker-core.js';

serveLibRaw(LibRawModule, {threaded: true});
