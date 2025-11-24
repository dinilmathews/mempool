#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>

#define MY_PRINT(...) printf(__VA_ARGS__)
#define MY_ASSERT(expr) assert(expr)

#else /* DEBUG */

#define MY_PRINT(...)
#define MY_ASSERT(expr)

#endif /* DEBUG */

#endif /* __DEBUG_H__ */