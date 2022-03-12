#include "cstring.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#define FORMAT_TEMP_SIZE 1024
#define inter_POOL_SIZE 1024
#define HASH_START_SIZE 16

struct stringno {
	struct cstring_data str;
	char buffer[Cstringinter_SIZE];
	struct stringno * next;
};

struct string_pool {
	struct stringno node[inter_POOL_SIZE];
};

struct stringinter {
	int lock;
	int size;
	struct stringno ** hash;
	struct string_pool * pool;
	int index;
	int total;
};

static struct stringinter S;

static inline void LOCK() {
	while (__sync_lock_test_and_set(&(S.lock),1)) {}
}

static inline void UNLOCK() {
	__sync_lock_release(&(S.lock));
}

static void insertno(struct stringno ** hash, int sz, struct stringno *n) {
	uint32_t h = n->str.hash_size;
	int index = h & (sz-1);
	n->next = hash[index];
	hash[index] = n;
}

static void ex(struct stringinter * si) {
	int new_size = si->size * 2;
	if (new_size < HASH_START_SIZE) {
		new_size = HASH_START_SIZE;
	}
	assert(new_size > si->total);
	struct stringno ** new_hash = malloc(sizeof(struct stringno *) * new_size);
	memset(new_hash, 0, sizeof(struct stringno *) * new_size);
	int i;
	for (i=0;i<si->size;i++) {
		struct stringno *node = si->hash[i];
		while (node) {
			struct stringno * tmp = node->next;
			insertno(new_hash, new_size, node);
			node = tmp;
		}
	}
	free(si->hash);
	si->hash = new_hash;
	si->size = new_size;
}

static cstring inter(struct stringinter * si, const char * cstr, size_t sz, uint32_t hash) {
	if (si->hash == NULL) {
		return NULL;
	}
	int index = (int)(hash & (si->size-1));
	struct stringno * n = si->hash[index];
	while(n) {
		if (n->str.hash_size == hash) {
			if (strcmp(n->str.cstr, cstr) == 0) {
				return &n->str;
			}
		}
		n = n->next;
	}

	if (si->total * 5 >= si->size * 4) {
		return NULL;
	}
	if (si->pool == NULL) {
		si->pool = malloc(sizeof(struct string_pool));
		assert(si->pool);
		si->index = 0;
	}
	n = &si->pool->node[si->index++];
	memcpy(n->buffer, cstr, sz);
	n->buffer[sz] = '\0';

	cstring cs = &n->str;
	cs->cstr = n->buffer;
	cs->hash_size = hash;
	cs->type = Cstringinter;
	cs->ref = 0;

	n->next = si->hash[index];
	si->hash[index] = n;

	return cs;
}

static cstring cstringinter(const char * cstr, size_t sz, uint32_t hash) {
	cstring ret;
	LOCK();
	ret = inter(&S, cstr, sz, hash);
	if (ret == NULL) {
		ex(&S);
		ret = inter(&S, cstr, sz, hash);
	}
	++S.total;
	UNLOCK();
	assert(ret);
	return ret;
}


static uint32_t hashbl(const char * buffer, size_t len) {
	const uint8_t * ptr = (const uint8_t *) buffer;
	size_t h = len;
	size_t step = (len>>5)+1;
	size_t i;
	for (i=len; i>=step; i-=step)
		h = h ^ ((h<<5)+(h>>2)+ptr[i-1]);
	if (h == 0)
		return 1;
	else
		return h;
}

void cstringfp(cstring s) {
	if (s->type == CSTRING_PERMANENT) {
		free(s);
	}
}

static cstring cstringcl(const char * cstr, size_t sz) {
	if (sz < Cstringinter_SIZE) {
		return cstringinter(cstr, sz, hashbl(cstr,sz));
	}
	struct cstring_data * p = malloc(sizeof(struct cstring_data) + sz + 1);
	assert(p);
	void * ptr = (void *)(p + 1);
	p->cstr = ptr;
	p->type = 0;
	p->ref = 1;
	memcpy(ptr, cstr, sz);
	((char *)ptr)[sz] = '\0';
	p->hash_size = 0;
	return p;
}

cstring cstringper(const char * cstr, size_t sz) {
	cstring s = cstringcl(cstr, sz);
	if (s->type == 0) {
		s->type = CSTRING_PERMANENT;
		s->ref = 0;
	}
	return s;
}

cstring cstringgr(cstring s) {
	if (s->type & (CSTRING_PERMANENT | Cstringinter)) {
		return s;
	}
	if (s->type == CSTRING_ONSTACK) {
		cstring tmp = cstringcl(s->cstr, s->hash_size);
		return tmp;
	} else {
		if (s->ref == 0) {
			s->type = CSTRING_PERMANENT;
		} else {
			__sync_add_and_fetch(&s->ref,1);
		}
		return s;
	}
}

void cstringrel(cstring s) {
	if (s->type != 0) {
		return;
	}
	if (s->ref == 0) {
		return;
	}
	if (__sync_sub_and_fetch(&s->ref,1) == 0) {
		free(s);
	}
}

uint32_t cstringhash(cstring s) {
	if (s->type == CSTRING_ONSTACK)
		return hashbl(s->cstr, s->hash_size);
	if (s->hash_size == 0) {
		s->hash_size = hashbl(s->cstr, strlen(s->cstr));
	}
	return s->hash_size;
}

int cstringeq(cstring a, cstring b) {
	if (a == b)
		return 1;
	if ((a->type == Cstringinter) &&
		(b->type == Cstringinter)) {
		return 0;
	}
	if ((a->type == CSTRING_ONSTACK) &&
		(b->type == CSTRING_ONSTACK)) {
		if (a->hash_size != b->hash_size) {
			return 0;
		}
		return memcmp(a->cstr, b->cstr, a->hash_size) == 0;
	}
	uint32_t hasha = cstringhash(a);
	uint32_t hashb = cstringhash(b);
	if (hasha != hashb) {
		return 0;
	}
	return strcmp(a->cstr, b->cstr) == 0;
}

static cstring cstringct2(const char * a, const char * b) {
	size_t sa = strlen(a);
	size_t sb = strlen(b);
	if (sa + sb < Cstringinter_SIZE) {
		char tmp[Cstringinter_SIZE];
		memcpy(tmp, a, sa);
		memcpy(tmp+sa, b, sb);
		tmp[sa+sb] = '\0';
		return cstringinter(tmp, sa+sb, hashbl(tmp,sa+sb));
	}
	struct cstring_data * p = malloc(sizeof(struct cstring_data) + sa + sb + 1);
	assert(p);
	char * ptr = (char *)(p + 1);
	p->cstr = ptr;
	p->type = 0;
	p->ref = 1;
	memcpy(ptr, a, sa);
	memcpy(ptr+sa, b, sb);
	ptr[sa+sb] = '\0';
	p->hash_size = 0;
	return p;
}

cstring cstringct(CSTRINGBUFF sb, const char * str) {
	cstring s = sb->str;
	if (s->type == CSTRING_ONSTACK) {
		int i = (int)s->hash_size;
		while (i < CSTRING_STACK_SIZE-1) {
			s->cstr[i] = *str;
			if (*str == '\0') {
				return s;
			}
			++s->hash_size;
			++str;
			++i;
		}
		s->cstr[i] = '\0';
	}
	cstring tmp = s;
	sb->str = cstringct2(tmp->cstr, str);
	cstringrel(tmp);
	return sb->str;
}

static cstring cstringf(const char * format, va_list ap) {
	static char * cache = NULL;
	char * result;
	char * temp = cache;
	if (temp) {
		temp = __sync_val_compare_and_swap(&cache, temp, NULL);
	}
	if (temp == NULL) {
		temp = (char *)malloc(FORMAT_TEMP_SIZE);
		assert(temp);
	}
	va_list ap2;
	va_copy(ap2, ap);
	int n = vsnprintf(temp, FORMAT_TEMP_SIZE, format, ap2);
	if (n >= FORMAT_TEMP_SIZE) {
		int sz = FORMAT_TEMP_SIZE * 2;
		for (;;) {
			result = malloc(sz);
			assert(result);
			va_copy(ap2, ap);
			n = vsnprintf(result, sz, format, ap2);
			if (n >= sz) {
				free(result);
				sz *= 2;
			} else {
				break;
			}
		}
	} else {
		result = temp;
	}
	cstring r = (cstring)malloc(sizeof(struct cstring_data) + n + 1);
	assert(r);
	r->cstr = (char *)(r+1);
	r->type = 0;
	r->ref = 1;
	r->hash_size = 0;
	memcpy(r->cstr, result, n+1);
	if (temp != result) {
		free(result);
	}
	if (!__sync_bool_compare_and_swap(&cache, NULL, temp)) {
		free(temp);
	} else {
	}

	return r;
}

cstring cstringprint(CSTRINGBUFF sb, const char * format, ...) {
	cstring s = sb->str;
	va_list ap;
	va_start(ap, format);
	if (s->type == CSTRING_ONSTACK) {
		int n = vsnprintf(s->cstr, CSTRING_STACK_SIZE, format, ap);
		if (n >= CSTRING_STACK_SIZE) {
			va_end(ap);
			va_start(ap, format);
			s = cstringf(format, ap);
			sb->str = s;
		} else {
			s->hash_size = n;
		}
	} else {
		cstringrel(sb->str);
		s = cstringf(format, ap);
		sb->str = s;
	}
	va_end(ap);
	return s;
}
