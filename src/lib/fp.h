#ifndef _FP_H_
#define _FP_H_

#include <stdint.h>
#define F 16384

#define FP_CONV_INT(X) ((X) * F)
#define FP_GET_INT(X) ((X) / F)
#define FP_GET_INT_RND(X) ((X) >= 0 ? (((X) + F / 2) / F) : (((X) - F / 2) / F))
#define FP_MUL(X, Y) ((int)(((int64_t)(X) * (Y)) / F))
#define FP_DIV(X, Y) ((int)(((int64_t)(X) * F) / Y))
#define FP_INC(X) ((X) + F)

#endif