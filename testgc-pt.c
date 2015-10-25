#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include "gc.h"

struct ololo {
	int a;
	int b;
	struct tralala *gcpointer t;
	char c;
};

struct tralala {
	char a[100];
};

GC_THRFUNC_DECLARE(t1func, void *param)
{
	struct tralala  *gcpointer t;

	t = GCNEW(struct tralala);
	memset(t->a, 0xFF, 100);

	return NULL;
}

GC_THRFUNC_DECLARE(t2func, void *param)
{
	struct ololo  *gcpointer o;

	o = GCNEW(struct ololo);
	o->a = 1;
	o->b = 2;
	o->t = GCNEW(struct tralala);
	o->c = 4;

	return NULL;
}

int main(void)
{
	pthread_t pt1, pt2;
	void *p;

	pthread_create(&pt1, NULL, t1func, NULL);
	pthread_create(&pt2, NULL, t2func, NULL);
	pthread_join(pt1, &p);
	pthread_join(pt2, &p);

	return 0;
}
