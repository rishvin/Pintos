#ifndef _FP_H_
#define _FP_H_

#include <stdint.h>

inline fp_t fp_conv_int(int x);
inline int fp_get_int(fp_t x);
inline int fp_get_frac(fp_t x);
inline int fp_get_int_rnd(fp_t x);
inline fp_t fp_mul(fp_t x, fp_t y);
inline fp_t fp_div(fp_t x, fp_t y);
inline fp_t fp_inc(fp_t x);
#endif
