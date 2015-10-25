#include <stdio.h>
#include "gc.h"

#define PRINTFUNC() \
	fprintf(stderr, "%s\n", __func__)

struct ololo {
	int a;
	int b;
	struct tralala *gcpointer t;
	char c;
};

struct tralala {
	char a[100];
};

void top(void)
{
	PRINTFUNC();
}

void bar(void)
{
	struct ololo   *gcpointer o1;
	struct tralala *gcpointer o2;

	PRINTFUNC();

	o1 = GCNEW(struct ololo);
	o1->t = GCNEW(struct tralala);
	o1 = GCNEW(struct tralala);

	top();
}

void foo(void)
{
	unsigned int  *gcpointer o1;
	unsigned long *gcpointer o2;
	unsigned char *gcpointer o3;

	PRINTFUNC();

	o1 = GCNEW(unsigned int);
	o2 = GCNEW(unsigned long);
	bar();
	o3 = GCNEW(unsigned char);
	bar();
}

int main(int argc, char *argv[])
{
	foo();

	return 0;
}
