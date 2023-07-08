#ifndef _include_util_h_
#define _include_util_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define ARRAYLEN(A) (sizeof(A)/sizeof(A[0]))

#define MIN(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _b : _a;          \
})

#define MAX(a, b)               \
({                              \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _a : _b;          \
})

extern int
WriteFile(
	const char* Path,
	uint64_t Length,
	uint8_t* Buffer
	);

extern int
ReadFile(
	const char* Path,
	uint64_t* Length,
	uint8_t** Buffer
	);

#ifdef __cplusplus
}
#endif

#endif /* _include_util_h_ */
