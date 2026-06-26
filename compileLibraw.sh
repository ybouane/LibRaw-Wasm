#!/usr/bin/env bash

set -e

#---------------------------------------------------------------------------------
# Stage A: Build the LCMS + LibRaw static libraries with Emscripten.
#
# These only change when the pinned versions (or this script) change, so when
# libs/ is already populated we reuse it and jump straight to linking the wrapper.
# Set FORCE_LIBS=1 to force a full rebuild (CI sets this automatically whenever
# compileLibraw.sh itself changes).
#---------------------------------------------------------------------------------
if [ "${FORCE_LIBS:-0}" = "1" ] || [ ! -f libs/libraw.a ] || [ ! -f libs/liblcms2.a ]; then
	echo -e "\n==> Building LCMS + LibRaw static libraries from source..."
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

	cp -R src/.libs/* ../libs/
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

	#-------------------------------------------------------------------------------
	# 2) Configure and Build LibRaw with Emscripten
	#-------------------------------------------------------------------------------
	echo -e "\n==> Configuring LibRaw with Emscripten..."
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
	emconfigure ./configure \
	  --host=wasm32-unknown-emscripten \
	  --enable-openmp \
	  --enable-lcms \
	  --enable-jpeg \
	  --disable-shared \
	  --disable-examples \
	  CFLAGS="-O3 -flto -ffast-math -msimd128 -DNDEBUG -DUSE_LCMS2 -DUSE_JPEG -DUSE_JPEG8 -sUSE_LIBJPEG=1 -I../includes" \
	  CXXFLAGS="-O3 -flto -ffast-math -msimd128 -DNDEBUG -DUSE_LCMS2 -DUSE_JPEG -DUSE_JPEG8 -sUSE_LIBJPEG=1 -I../includes" \
	  LDFLAGS="-s USE_PTHREADS=1 -sUSE_LIBJPEG=1 -lpthread -L../libs/ -llcms2"

	echo -e "\n==> Building LibRaw..."
	emmake make -j8

	# Copy artifacts out of the source folder for convenience
	cp -R lib/.libs/* ../libs/
	cp -R libraw ../includes/
	popd  # out of LibRawSource
else
	echo -e "\n==> Reusing existing libs/ + includes/ (set FORCE_LIBS=1 to rebuild them)."
fi

#---------------------------------------------------------------------------------
# Stage B: Build the final WASM from libraw_wrapper.cpp (always runs).
#---------------------------------------------------------------------------------
echo -e "\n==> Building libraw.js + libraw.wasm..."
emcc \
  --bind \
  -I./includes \
  -s USE_LIBPNG=1 \
  -s USE_LIBJPEG=1 \
  -s USE_ZLIB=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s DISABLE_EXCEPTION_CATCHING=0 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=256MB \
  -s USE_PTHREADS=1 \
  -s ENVIRONMENT="web,worker" \
  -msimd128 \
  -O3 -flto -pthread \
  libraw_wrapper.cpp \
  ./libs/liblcms2.a \
  ./libs/libraw.a \
  -o libraw.js


echo -e "\n==> Building Dist files..."

node build.js


echo ""
echo "==============================================="
echo " Build complete!"
echo " You should now have libraw.js & libraw.wasm."
echo "==============================================="
