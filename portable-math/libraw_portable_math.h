/*
 * Force-included (-include) into every LibRaw C++ source by compileLibraw.sh.
 *
 * Sends LibRaw's transcendental libm calls - pow, powf, exp, log, logf and cos,
 * the complete set LibRaw 0.22.1 uses - to the musl copies bundled in
 * portable-math/musl, so the decoded pixels do not depend on which libm (or
 * Emscripten version) the build links, and no compiler can constant-fold or
 * rewrite those calls using its own assumptions about them. See README.md.
 *
 * The overloads mirror the ones <cmath> gives these names in C++:
 * pow(float, float) is powf, log(float) is logf, and mixed or integer
 * arguments are promoted to double. LibRaw never calls exp or cos with a float
 * (it would get expf/cosf), so those overloads are deleted: a new call like
 * that fails to compile instead of silently changing behaviour.
 *
 * Does nothing for C sources (e.g. configure's test programs).
 */
#ifndef LIBRAW_PORTABLE_MATH_H
#define LIBRAW_PORTABLE_MATH_H

#ifdef __cplusplus

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES /* M_PI on MSVC, as LibRaw's internal/defines.h expects */
#endif
#include <math.h>
#include <cmath>
#include <type_traits>

extern "C" {
double libraw_portable_pow(double, double);
float libraw_portable_powf(float, float);
double libraw_portable_exp(double);
double libraw_portable_log(double);
float libraw_portable_logf(float);
double libraw_portable_cos(double);
}

namespace libraw_portable_math {

inline float pow(float x, float y) { return libraw_portable_powf(x, y); }
inline double pow(double x, double y) { return libraw_portable_pow(x, y); }
template <class A, class B>
inline double pow(A x, B y)
{
  static_assert(std::is_arithmetic<A>::value && std::is_arithmetic<B>::value &&
                    !std::is_same<A, long double>::value && !std::is_same<B, long double>::value,
                "portable-math: pow() needs float, double or integer arguments");
  return libraw_portable_pow(double(x), double(y));
}

inline double exp(double x) { return libraw_portable_exp(x); }
template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline double exp(T x) { return libraw_portable_exp(double(x)); }
float exp(float) = delete;
long double exp(long double) = delete;

inline float log(float x) { return libraw_portable_logf(x); }
inline double log(double x) { return libraw_portable_log(x); }
template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline double log(T x) { return libraw_portable_log(double(x)); }
long double log(long double) = delete;

inline double cos(double x) { return libraw_portable_cos(x); }
template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline double cos(T x) { return libraw_portable_cos(double(x)); }
float cos(float) = delete;
long double cos(long double) = delete;

} // namespace libraw_portable_math

#define pow(x, y) libraw_portable_math::pow(x, y)
#define powf(x, y) libraw_portable_powf(x, y)
#define exp(x) libraw_portable_math::exp(x)
#define log(x) libraw_portable_math::log(x)
#define logf(x) libraw_portable_logf(x)
#define cos(x) libraw_portable_math::cos(x)

#endif /* __cplusplus */

#endif /* LIBRAW_PORTABLE_MATH_H */
