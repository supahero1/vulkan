#ifndef _include_debug_h_
#define _include_debug_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#define GetPrintfType(X)	\
_Generic((X),				\
	int8_t:			PRId8,	\
	int16_t:		PRId16,	\
	int32_t:		PRId32,	\
	int64_t:		PRId64,	\
	uint8_t:		PRIu8,	\
	uint16_t:		PRIu16,	\
	uint32_t:		PRIu32,	\
	uint64_t:		PRIu64,	\
	float:			"f",	\
	double:			"lf",	\
	long double:	"Lf",	\
	default:		"p"		\
)

#define AssertMsg(Op, Desc)	\
"Assertion \"%%s " Op " %%s\" failed: '%%%s' " Desc " '%%%s', at %%s:%%d.\n"

#define HardenedAssertEQ(A, B)						\
do													\
{													\
	__typeof__(A) a = (A);							\
	__typeof__(B) b = (B);							\
													\
	if(__builtin_expect(a != b, 0))					\
	{												\
		char Fmt[4096];								\
		sprintf(Fmt, AssertMsg("==", "is not"),		\
			GetPrintfType(A), GetPrintfType(B));	\
		fprintf(stderr, Fmt, #A, #B,				\
			a, b, __FILE__, __LINE__);				\
		abort();									\
	}												\
}													\
while(0)

#define HardenedAssertNEQ(A, B)						\
do													\
{													\
	__typeof__(A) a = (A);							\
	__typeof__(B) b = (B);							\
													\
	if(__builtin_expect(a == b, 0))					\
	{												\
		char Fmt[4096];								\
		sprintf(Fmt, AssertMsg("!=", "is"),			\
			GetPrintfType(A), GetPrintfType(B));	\
		fprintf(stderr, Fmt, #A, #B,				\
			a, b, __FILE__, __LINE__);				\
		abort();									\
	}												\
}													\
while(0)

#define EmptyAssertEQ(A, B)							\
do													\
{													\
	__typeof__(A) a = (A);							\
	__typeof__(B) b = (B);							\
													\
	if(__builtin_expect(a != b, 0))					\
	{												\
		__builtin_unreachable();					\
	}												\
}													\
while(0)

#define EmptyAssertNEQ(A, B)						\
do													\
{													\
	__typeof__(A) a = (A);							\
	__typeof__(B) b = (B);							\
													\
	if(__builtin_expect(a == b, 0))					\
	{												\
		__builtin_unreachable();					\
	}												\
}													\
while(0)

#ifndef NDEBUG
	#define AssertEQ(A, B) HardenedAssertEQ(A, B)
	#define AssertNEQ(A, B) HardenedAssertNEQ(A, B)
#else
	#define AssertEQ(A, B) EmptyAssertEQ(A, B)
	#define AssertNEQ(A, B) EmptyAssertNEQ(A, B)
#endif /* NDEBUG */


#ifdef __cplusplus
}
#endif

#endif /* _include_debug_h_ */
