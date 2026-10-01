import LibRawModule from './libraw.js';
import serveLibRaw from './worker-core.js';

serveLibRaw(LibRawModule, {threaded: false});
