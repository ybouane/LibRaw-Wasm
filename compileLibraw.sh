#!/usr/bin/env bash

set -e

ROOT="$(pwd)"

#---------------------------------------------------------------------------------
# Floating-point settings for LibRaw (see "Deterministic output" in readme.md).
#
# Both builds compile LibRaw with the same IEEE-strict settings, so they decode
# to identical pixels, and the multi-threaded build does so at any thread count.
# (One exception: with wavelet denoising, threshold > 0, LibRaw's OpenMP variant
# of wavelet_denoise() does part of its arithmetic in double instead of float,
# so the two builds differ by a few pixels there; each is still deterministic.)
#  - no -ffast-math: it lets the compiler reassociate and assume there are no
#    NaNs/infinities, which made the output depend on the compiler version;
#  - -ffp-contract=off: no fused multiply-add contraction. wasm32 has no FMA, so
#    this is a no-op here, but it keeps the flags valid for a native build of
#    the same sources (arm64 contracts by default);
#  - portable-math/: LibRaw's pow/powf/exp/log/logf/cos calls are redirected to
#    bundled, unmodified musl v1.2.6 copies (compiled below), so neither the
#    libm that gets linked nor the compiler's own folding or rewriting of those
#    calls can change the output.
#---------------------------------------------------------------------------------
PORTABLE_MATH="${ROOT}/portable-math"
LIBRAW_FLAGS="-O3 -flto -msimd128 -ffp-contract=off -include ${PORTABLE_MATH}/libraw_portable_math.h -DNDEBUG -DUSE_LCMS2 -DUSE_JPEG -DUSE_JPEG8 -sUSE_LIBJPEG=1 -I${ROOT}/includes"
PORTABLE_MATH_FLAGS="-O2 -flto -msimd128 -ffp-contract=off -fno-builtin -U__FP_FAST_FMA -U__FP_FAST_FMAF -I${PORTABLE_MATH}/include -include ${PORTABLE_MATH}/include/libm.h"

#---------------------------------------------------------------------------------
# Stage A: Build the LCMS, LibRaw and portable-math static libraries.
#
# These only change when the pinned versions (or this script, or portable-math/)
# change, so when libs/ is already populated we reuse it and jump straight to
# linking the wrapper. Set FORCE_LIBS=1 to force a full rebuild (CI sets this
# automatically whenever compileLibraw.sh or portable-math/ changes).
#---------------------------------------------------------------------------------
if [ "${FORCE_LIBS:-0}" = "1" ] || [ ! -f libs/libraw.a ] || [ ! -f libs/libraw-openmp.a ] \
	|| [ ! -f libs/liblcms2.a ] || [ ! -f libs/libportablemath.a ]; then
	echo -e "\n==> Building LCMS, LibRaw and portable-math static libraries from source..."
	rm -rf libs includes LibRawSource lcms2 2>/dev/null || true
	mkdir libs
	mkdir includes

	#-------------------------------------------------------------------------------
	# 0) Configure and Build LCMS with Emscripten
	#-------------------------------------------------------------------------------
	echo -e "\n==> Cloning LCMS from GitHub (lcms2.19.1)..."
	git clone --branch lcms2.19.1 --depth 1 https://github.com/mm2/Little-CMS.git lcms2
	cd lcms2
	command -v libtoolize >/dev/null 2>&1 && libtoolize || glibtoolize # MacOS fallback

	autoreconf -fi
	# 2) Configure and make with Emscripten
	emconfigure ./configure --host=wasm32-unknown-emscripten \
	  --disable-shared
	emmake make -j8

	cp src/.libs/liblcms2.a ../libs/
	cp -R include/* ../includes/
	cd ..

	#-------------------------------------------------------------------------------
	# 1) Download & Prepare LibRaw
	#-------------------------------------------------------------------------------
	echo -e "\n==> Cloning LibRaw from GitHub (0.22.1)..."
	git clone --branch 0.22.1 --depth 1 https://github.com/LibRaw/LibRaw.git LibRawSource

	pushd LibRawSource

	echo -e "\n==> Generating configure script from configure.ac..."
	# Generate ./configure from configure.ac
	command -v libtoolize >/dev/null 2>&1 && libtoolize || glibtoolize # MacOS fallback
	autoreconf -i
	popd

	#-------------------------------------------------------------------------------
	# 2) Configure and Build LibRaw with Emscripten, twice (out of tree)
	#-------------------------------------------------------------------------------
	# JPEG support (USE_JPEG / USE_JPEG8) is what enables lossy DNG (Adobe lossy /
	# baseline-JPEG, compression 34892) and Kodak JPEG RAW decoding — without it
	# LibRaw compiles lossy_dng_load_raw()/kodak_jpeg_load_raw() as empty stubs and
	# imageData() silently returns nothing (see issue #27).
	#
	# LibRaw's configure enables jpeg by default but only defines USE_JPEG when its
	# AC_CHECK_LIB([jpeg], jpeg_mem_src) + AC_CHECK_HEADERS([jpeglib.h]) both pass.
	# Under Emscripten those live in the libjpeg port, so we surface them with
	# -sUSE_LIBJPEG=1 (adds the jpeglib.h include path at compile time and the jpeg
	# archive at link time) and additionally force -DUSE_JPEG -DUSE_JPEG8 so the
	# decoder code is compiled into libraw.a regardless of the configure link probe.
	# The jpeg symbols themselves are resolved later when Stage B links the wrapper
	# with -s USE_LIBJPEG=1.
	#
	# libraw.a is the single-threaded build. --disable-openmp matters: since
	# Emscripten 6.0.3 ships an OpenMP runtime, configure's probe would otherwise
	# succeed and quietly add -fopenmp.
	#
	# libraw-openmp.a compiles LibRaw's OpenMP regions in (-fopenmp). It uses
	# LIBRAW_FORCE_OPENMP rather than LIBRAW_USE_OPENMP because libraw_types.h
	# undefines the latter for some platforms; FORCE always wins.
	#
	# --disable-zlib keeps deflate-compressed DNG support out, as it has always
	# been in the published package. LibRaw's configure enables zlib whenever
	# pkg-config finds it, and the Emscripten cache has a zlib.pc as soon as the
	# zlib port has been used once (Stage B uses it), so without the flag a
	# second build on the same machine would silently compile a different
	# libraw.a than a fresh one (CI always starts from a fresh cache).
	build_libraw() {
		local variant="$1" configure_flags="$2" compile_flags="$3" link_flags="$4" archive="$5"
		echo -e "\n==> Configuring and building LibRaw (${variant})..."
		mkdir -p "LibRawSource/build-${variant}"
		pushd "LibRawSource/build-${variant}"
		emconfigure ../configure \
		  --host=wasm32-unknown-emscripten \
		  ${configure_flags} \
		  --enable-lcms \
		  --enable-jpeg \
		  --disable-zlib \
		  --disable-shared \
		  --disable-examples \
		  CFLAGS="${LIBRAW_FLAGS} ${compile_flags}" \
		  CXXFLAGS="${LIBRAW_FLAGS} ${compile_flags}" \
		  LDFLAGS="-pthread ${link_flags} -sUSE_LIBJPEG=1 -L${ROOT}/libs/ -llcms2"
		emmake make -j8 lib/libraw.la
		cp lib/.libs/libraw.a "${ROOT}/libs/${archive}"
		popd
	}
	build_libraw single "--disable-openmp" "" "" libraw.a
	build_libraw openmp "--enable-openmp" "-fopenmp -DLIBRAW_FORCE_OPENMP" "-fopenmp" libraw-openmp.a
	cp -R LibRawSource/libraw includes/

	#-------------------------------------------------------------------------------
	# 3) Build the portable math functions (portable-math/musl) that LibRaw calls
	#-------------------------------------------------------------------------------
	echo -e "\n==> Building portable-math..."
	PORTABLE_MATH_OBJS="$(mktemp -d)"
	for src in portable-math/musl/*.c; do
		emcc -c ${PORTABLE_MATH_FLAGS} "${src}" -o "${PORTABLE_MATH_OBJS}/$(basename "${src}" .c).o"
	done
	emar rcs libs/libportablemath.a "${PORTABLE_MATH_OBJS}"/*.o
	rm -rf "${PORTABLE_MATH_OBJS}"
else
	echo -e "\n==> Reusing existing libs/ + includes/ (set FORCE_LIBS=1 to rebuild them)."
fi

#---------------------------------------------------------------------------------
# Stage B: Build the final WASM from libraw_wrapper.cpp (always runs), twice:
#  - libraw.js + libraw.wasm: single-threaded (the default `new LibRaw()`);
#  - libraw-threaded.js + libraw-threaded.wasm: LibRaw's OpenMP regions on a
#    pthread pool (`new LibRaw({ threads })`). The pool must exist before a
#    decode starts, because a worker that blocks on its result cannot start new
#    threads, so its size is a JS expression the worker fills in per instance
#    (worker-threaded.js), and running out of pool workers is an error
#    (PTHREAD_POOL_SIZE_STRICT=2) rather than a deadlock. MAXIMUM_MEMORY=4GB
#    lifts the default 2GB cap, which we estimate a ~100 MP decode would hit
#    (raw data, 4-channel image and output buffer); not yet tested above 2GB.
#---------------------------------------------------------------------------------
LINK_FLAGS=(
  --bind
  -I./includes
  -s USE_LIBPNG=1
  -s USE_LIBJPEG=1
  -s USE_ZLIB=1
  -s MODULARIZE=1
  -s EXPORT_ES6=1
  -s DISABLE_EXCEPTION_CATCHING=0
  -s ALLOW_MEMORY_GROWTH=1
  -s INITIAL_MEMORY=256MB
  -s ENVIRONMENT="web,worker"
  -msimd128
  -O3 -flto -pthread
)

echo -e "\n==> Building libraw.js + libraw.wasm (single-threaded)..."
em++ \
  "${LINK_FLAGS[@]}" \
  libraw_wrapper.cpp \
  ./libs/liblcms2.a \
  ./libs/libraw.a \
  ./libs/libportablemath.a \
  -o libraw.js

echo -e "\n==> Building libraw-threaded.js + libraw-threaded.wasm (OpenMP)..."
em++ \
  "${LINK_FLAGS[@]}" \
  -fopenmp \
  -s MAXIMUM_MEMORY=4GB \
  -s PTHREAD_POOL_SIZE='Module["librawThreadPoolSize"]|0' \
  -s PTHREAD_POOL_SIZE_STRICT=2 \
  libraw_wrapper.cpp \
  ./libs/liblcms2.a \
  ./libs/libraw-openmp.a \
  ./libs/libportablemath.a \
  -o libraw-threaded.js


echo -e "\n==> Building Dist files..."

node build.js


echo ""
echo "==============================================="
echo " Build complete!"
echo " You should now have libraw.js & libraw.wasm"
echo " and libraw-threaded.js & libraw-threaded.wasm."
echo "==============================================="
