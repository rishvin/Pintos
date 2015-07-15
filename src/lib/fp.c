#include "fp.h"

#define F (1 << 14)

fp_t fp_int_to_fp_t(int x)
{
	return (fp_t)x * F;
}

int fp_fp_t_to_int(fp_t x)
{
	return (int)(x / F);
}

int fp_div_round_to_int(fp_t x)
{
	if(x >= 0)
		return (int)((x + F / 2) / F);
	return (int)((x - F / 2) / F);
}

fp_t fp_round_to_int(fp_t x)
{
    return x / F;
}

fp_t fp_adjust(fp_t x)
{
    return x / F;
}