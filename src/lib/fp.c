#include "fp.h"

#define F 16384

fp_t fp_conv_int(int x)
{
	return (fp_t)x * F;
}

int fp_get_int(fp_t x)
{
	return (int)(x / F);
}

int fp_get_frac(fp_t x)
{
    return x & FP_MAX_FRACTION;
}

int fp_get_int_rnd(fp_t x)
{
	if((int)x >= 0)
		return (int)((x + F / 2) / F);
	return (int)((x - F / 2) / F);
}

fp_t fp_mul(fp_t x, fp_t y)
{
    return (int64_t)x * y / F;
}

fp_t fp_div(fp_t x, fp_t y)
{
    return (int64_t)x * F / y;
}

fp_t fp_inc(fp_t x)
{
    return x + 16384;
}