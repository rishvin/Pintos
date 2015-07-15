#ifndef _FP_H_
#define _FP_H_

#define MAX_FP_FRACTIONAL (1 << 15 - 1)

typedef long long fp_t;


fp_t fp_int_to_fp_t(int x);
int fp_fp_t_to_int(fp_t x);
int fp_div_round_to_int(fp_t x);
fp_t fp_round_to_int(fp_t x);
fp_t fp_adjust(fp_t x);

#endif
