# portable-math

LibRaw builds lookup tables with libm on every decode: the output gamma curve
(`pow`, `exp`, `log`), the CIELab cube-root table AHD uses to pick directions
(`pow(float, float)`, i.e. `powf`), and a few smaller ones (`logf`, `cos`).
Different libms, and compilers that constant-fold or rewrite these calls
(`pow(x, 0.5)` → `sqrt`, folding with the build machine's own libm, …), can
shift those tables by one unit in the last place, which can move a decoded
pixel by a level. This directory pins them.

- `musl/` — musl v1.2.6's `pow`, `powf`, `exp`, `log`, `logf` and `cos`, with the
  data tables and internal helpers they use, **unmodified** (`musl-1.2.6.tar.gz`,
  sha256 `d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a`,
  `src/math/`). They are the same files Emscripten's libc compiles (5.0.7 and
  6.0.4 alike), so on wasm32 they compute exactly what earlier builds of this
  package computed. musl is MIT licensed; see `musl/COPYRIGHT` and the notices in
  the individual files.
- `include/libm.h` — musl's `src/internal/libm.h` (MIT, see `musl/COPYRIGHT`),
  trimmed to what these files use, so they build outside musl. On top of musl's
  header it renames every function and table to `libraw_portable_*` (so nothing
  clashes with, or interposes on, the platform libm) and undefines
  `__FP_FAST_FMA`, so every target takes musl's non-FMA code paths — the only
  ones wasm32, which has no fused multiply-add, can take. `include/features.h`
  is new: it provides only the `hidden` macro the files take from musl's own
  `<features.h>`. `__rem_pio2_large.c` also calls `floor` and `scalbn`; both are
  exact on every libm.
- `libraw_portable_math.h` — force-included (`-include`) into LibRaw's C++
  sources. It maps LibRaw's calls onto the functions above, with the same
  overloads `<cmath>` gives these names (`pow(float, float)` is `powf`,
  `log(float)` is `logf`, other arguments promote to `double`).

`compileLibraw.sh` compiles each file in `musl/` as C with `PORTABLE_MATH_FLAGS`:

```
-O2 -flto -msimd128 -ffp-contract=off -fno-builtin -U__FP_FAST_FMA -U__FP_FAST_FMAF -I portable-math/include -include portable-math/include/libm.h
```

into `libs/libportablemath.a` (`-I` resolves the files' own `#include "libm.h"`
and `<features.h>`; the forced include matters, since the `*_data.c` files do not
include `libm.h` themselves), and every LibRaw source with `-ffp-contract=off
-include portable-math/libraw_portable_math.h` (`LIBRAW_FLAGS`). A native build
of LibRaw that should decode to the same pixels as this package does the same,
minus `-flto -msimd128`, and must not use `-ffast-math`/`/fp:fast` or FMA
contraction.

To check that `musl/` is unmodified:

```sh
curl -sLO https://musl.libc.org/releases/musl-1.2.6.tar.gz && echo "d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a  musl-1.2.6.tar.gz" | shasum -a 256 -c && tar xzf musl-1.2.6.tar.gz && for f in portable-math/musl/*.c portable-math/musl/*.h; do cmp "$f" "musl-1.2.6/src/math/$(basename "$f")" || exit 1; done && cmp portable-math/musl/COPYRIGHT musl-1.2.6/COPYRIGHT && echo "portable-math/musl is unmodified musl 1.2.6"
```

To update musl, copy the same files from the new release's `src/math/`, and
check its `src/internal/libm.h` for changes that `include/libm.h` should follow.
