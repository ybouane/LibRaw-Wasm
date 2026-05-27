#!/usr/bin/env bash

set -e

rm -rf libs includes LibRawSource lcms2 2>/dev/null || true
mkdir libs
mkdir includes


#---------------------------------------------------------------------------------
# 0) Configure and Build LCMS with Emscripten
#---------------------------------------------------------------------------------
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



#---------------------------------------------------------------------------------
# 1) Download & Prepare LibRaw
#---------------------------------------------------------------------------------
echo -e "\n==> Cloning LibRaw from GitHub (0.22.1)..."
git clone --branch 0.22.1 --depth 1 https://github.com/LibRaw/LibRaw.git LibRawSource

pushd LibRawSource

echo -e "\n==> Generating configure script from configure.ac..."
# Generate ./configure from configure.ac
command -v libtoolize >/dev/null 2>&1 && libtoolize || glibtoolize # MacOS fallback
autoreconf -i

#---------------------------------------------------------------------------------
# 2) Configure and Build LibRaw with Emscripten
#---------------------------------------------------------------------------------
echo -e "\n==> Configuring LibRaw with Emscripten..."
emconfigure ./configure \
  --host=wasm32-unknown-emscripten \
  --enable-openmp \
  --enable-lcms \
  --disable-shared \
  --disable-examples \
  CFLAGS="-O3 -flto -ffast-math -msimd128 -DNDEBUG -DUSE_LCMS2 -I../includes" \
  CXXFLAGS="-O3 -flto -ffast-math -msimd128 -DNDEBUG -DUSE_LCMS2 -I../includes" \
  LDFLAGS="-s USE_PTHREADS=1 -lpthread -L../libs/ -llcms2"

echo -e "\n==> Building LibRaw..."
emmake make -j8

# Copy artifacts out of the source folder for convenience
cp -R lib/.libs/* ../libs/
cp -R libraw ../includes/
popd  # out of LibRawSource

#---------------------------------------------------------------------------------
# 3) Build the final WASM from libraw_wrapper.cpp
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
