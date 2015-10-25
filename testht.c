#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"

struct DataElem {
	struct HashEntry he;
	unsigned int data;
};

struct DataElem elems[] = {
	{ .data = 1},
	{ .data = 100},
	{ .data = 1000},
	{ .data = 10000},
	{ .data = 100000},
	{ .data = 1000000},
	{ .data = 10000000},
	{ .data = 100000000},
};

int main(int argc, char *argv[])
{
	struct HashTable *ht;

	ht = htalloc(128);

	{
		int i, c = 8;
		struct DataElem *elem;
		for (i = 0; i < 8; i++) {
			elem = &elems[i];
			htadd(ht, (struct HashEntry *)elem, elem->data);
		}
	}

	htprint(ht);

	htdel(ht, 100000);
	htdel(ht, 1);

	htprint(ht);

	return 0;
}
