#ifndef cstring_h
#define cstring_h

#include <stdint.h>
#include <stddef.h>

#define CSTRING_PERMANENT 1
#define Cstringinter 2
#define CSTRING_ONSTACK 4

#define Cstringinter_SIZE 32
#define CSTRING_STACK_SIZE 128

struct cstring_data {
	char * cstr;
	uint32_t hash_size;
	uint16_t type;
	uint16_t ref;
};

typedef struct _CSTRINGBUFF {
	struct cstring_data * str;
} CSTRINGBUFF[1];

typedef struct cstring_data * cstring;

#define CSTRINGBUFF(var) \
	char var##_cstring [CSTRING_STACK_SIZE] = { '\0' };	\
	struct cstring_data var##_cstring_data = { var##_cstring , 0, CSTRING_ONSTACK, 0 };	\
	CSTRINGBUFF var;	\
	var->str = &var##_cstring_data;

#define CSTRINGLIT(var, cstr)	\
	static cstring var = NULL;	\
	if (var) {} else {	\
		cstring tmp = cstringper(""cstr, (sizeof(cstr)/sizeof(char))-1);	\
		if (!__sync_bool_compare_and_swap(&var, NULL, tmp)) {	\
			cstringfp(tmp);	\
		}	\
	}

#define CSTRING(s) ((s)->str)

#define CSTRINGEND(var) \
	if ((var)->str->type != 0) {} else \
		cstringrel((var)->str);

cstring cstringper(const char * cstr, size_t sz);
void cstringfp(cstring s);

cstring cstringgr(cstring s);
void cstringrel(cstring s);
cstring cstringct(CSTRINGBUFF sb, const char * str);
cstring cstringprint(CSTRINGBUFF sb, const char * format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
;
int cstringeq(cstring a, cstring b);
uint32_t cstringhash(cstring s);

#endif


