/*
 * Stand-in for musl's src/internal/libm.h, used to compile the vendored musl
 * v1.2.6 math sources (../musl/) outside musl, identically on every target.
 *
 * On top of musl's header it:
 *  - undefines __FP_FAST_FMA, so every target takes musl's non-FMA code
 *    paths: the only ones wasm32, which has no fused multiply-add, can take;
 *  - renames the public functions, internal helpers and data tables to
 *    libraw_portable_*, so they never clash with, or interpose on, the
 *    platform's libm.
 *
 * The rest is musl's libm.h (MIT, see ../musl/COPYRIGHT) trimmed to what the
 * vendored files use, including Emscripten's no-op FORCE_EVAL. FORCE_EVAL and
 * the fp_barrier helpers only exist to raise floating-point exceptions; they
 * never change a returned value.
 */
#ifndef _LIBM_H
#define _LIBM_H

#include <stdint.h>
#include <float.h>
#include <math.h>
#include <features.h>

#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0
#error "portable-math needs FLT_EVAL_METHOD == 0 (no excess precision): build for wasm32, x86-64 (SSE2) or arm64"
#endif

#undef __FP_FAST_FMA
#undef __FP_FAST_FMAF

#define pow libraw_portable_pow
#define powf libraw_portable_powf
#define exp libraw_portable_exp
#define log libraw_portable_log
#define logf libraw_portable_logf
#define cos libraw_portable_cos
#define __cos libraw_portable___cos
#define __sin libraw_portable___sin
#define __rem_pio2 libraw_portable___rem_pio2
#define __rem_pio2_large libraw_portable___rem_pio2_large
#define __math_oflow libraw_portable___math_oflow
#define __math_uflow libraw_portable___math_uflow
#define __math_xflow libraw_portable___math_xflow
#define __math_invalid libraw_portable___math_invalid
#define __math_divzero libraw_portable___math_divzero
#define __math_oflowf libraw_portable___math_oflowf
#define __math_uflowf libraw_portable___math_uflowf
#define __math_xflowf libraw_portable___math_xflowf
#define __math_invalidf libraw_portable___math_invalidf
#define __math_divzerof libraw_portable___math_divzerof
#define __exp_data libraw_portable___exp_data
#define __exp2f_data libraw_portable___exp2f_data
#define __log_data libraw_portable___log_data
#define __logf_data libraw_portable___logf_data
#define __pow_log_data libraw_portable___pow_log_data
#define __powf_log2_data libraw_portable___powf_log2_data

/* Support non-nearest rounding mode.  */
#define WANT_ROUNDING 1
/* Support signaling NaNs.  */
#define WANT_SNAN 0

#define issignalingf_inline(x) 0
#define issignaling_inline(x) 0

#define TOINT_INTRINSICS 0

/* Helps static branch prediction so hot path can be better optimized.  */
#ifdef __GNUC__
#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)
#else
#define predict_true(x) (x)
#define predict_false(x) (x)
#endif

/* Evaluate an expression as the specified type. With standard excess
   precision handling a type cast or assignment is enough (with
   -ffloat-store an assignment is required, in old compilers argument
   passing and return statement may not drop excess precision).  */

static inline float eval_as_float(float x)
{
	float y = x;
	return y;
}

static inline double eval_as_double(double x)
{
	double y = x;
	return y;
}

/* fp_barrier returns its input, but limits code transformations
   as if it had a side-effect (e.g. observable io) and returned
   an arbitrary value.  */

static inline float fp_barrierf(float x)
{
	volatile float y = x;
	return y;
}

static inline double fp_barrier(double x)
{
	volatile double y = x;
	return y;
}

/* fp_force_eval ensures that the input value is computed when that's
   otherwise unused.  */

static inline void fp_force_evalf(float x)
{
	volatile float y;
	y = x;
}

static inline void fp_force_eval(double x)
{
	volatile double y;
	y = x;
}

#ifdef __EMSCRIPTEN__
#define FORCE_EVAL(x)
#else
#define FORCE_EVAL(x) do {                        \
	if (sizeof(x) == sizeof(float)) {         \
		fp_force_evalf(x);                \
	} else {                                  \
		fp_force_eval(x);                 \
	}                                         \
} while(0)
#endif

#define asuint(f) ((union{float _f; uint32_t _i;}){f})._i
#define asfloat(i) ((union{uint32_t _i; float _f;}){i})._f
#define asuint64(f) ((union{double _f; uint64_t _i;}){f})._i
#define asdouble(i) ((union{uint64_t _i; double _f;}){i})._f

#define EXTRACT_WORDS(hi,lo,d)                    \
do {                                              \
  uint64_t __u = asuint64(d);                     \
  (hi) = __u >> 32;                               \
  (lo) = (uint32_t)__u;                           \
} while (0)

#define GET_HIGH_WORD(hi,d)                       \
do {                                              \
  (hi) = asuint64(d) >> 32;                       \
} while (0)

#define GET_LOW_WORD(lo,d)                        \
do {                                              \
  (lo) = (uint32_t)asuint64(d);                   \
} while (0)

#define INSERT_WORDS(d,hi,lo)                     \
do {                                              \
  (d) = asdouble(((uint64_t)(hi)<<32) | (uint32_t)(lo)); \
} while (0)

#define SET_HIGH_WORD(d,hi)                       \
  INSERT_WORDS(d, hi, (uint32_t)asuint64(d))

#define SET_LOW_WORD(d,lo)                        \
  INSERT_WORDS(d, asuint64(d)>>32, lo)

#define GET_FLOAT_WORD(w,d)                       \
do {                                              \
  (w) = asuint(d);                                \
} while (0)

#define SET_FLOAT_WORD(d,w)                       \
do {                                              \
  (d) = asfloat(w);                               \
} while (0)

hidden int    __rem_pio2_large(double*,double*,int,int,int);
hidden int    __rem_pio2(double,double*);
hidden double __sin(double,double,int);
hidden double __cos(double,double);

/* error handling functions */
hidden float __math_xflowf(uint32_t, float);
hidden float __math_uflowf(uint32_t);
hidden float __math_oflowf(uint32_t);
hidden float __math_divzerof(uint32_t);
hidden float __math_invalidf(float);
hidden double __math_xflow(uint32_t, double);
hidden double __math_uflow(uint32_t);
hidden double __math_oflow(uint32_t);
hidden double __math_divzero(uint32_t);
hidden double __math_invalid(double);

#endif
