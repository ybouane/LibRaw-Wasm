/*
 * Stand-in for the one thing the vendored musl math sources (../musl/) take
 * from musl's internal <features.h>: the `hidden` visibility macro.
 *
 * It is only on the include path while those files are compiled. Where the
 * platform has its own <features.h> (glibc, Emscripten's musl) it is included
 * first, so system headers that include <features.h> keep working.
 */
#if defined(__has_include_next)
#if __has_include_next(<features.h>)
#include_next <features.h>
#endif
#endif

#ifndef LIBRAW_PORTABLE_MATH_FEATURES_H
#define LIBRAW_PORTABLE_MATH_FEATURES_H

#ifndef hidden
#if defined(__GNUC__) || defined(__clang__)
#define hidden __attribute__((__visibility__("hidden")))
#else
#define hidden
#endif
#endif

#endif
