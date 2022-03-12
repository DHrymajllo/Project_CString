#include "cstring.h"

#include <stdio.h>

static cstring foo(cstring t) {
	CSTRINGLIT(test, "\"test string\"");
	printf("%s", test->cstr);
	CSTRINGBUFF(ret);
	if (cstringeq(test,t)) {
		cstringct(ret, "equal");
	} else {
		cstringct(ret, "not equal");
	}
	return cstringgr(CSTRING(ret));
}

static void test() {
	CSTRINGBUFF(a);
	printf("Test1:\n");
	cstringprint(a, "%s", "\"test string\"");
	printf("%s == ", CSTRING(a)->cstr);
	cstring b = foo(CSTRING(a));
	printf("\nStatus: %s\n", b->cstr);
	cstringprint(a, "\nTest 2:\nlong string %s",
              "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
	printf("%s\n", CSTRING(a)->cstr);
	CSTRINGEND(a);
	cstringrel(b);
}

int main() {
	test();
	return 0;
}
