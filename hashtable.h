#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

struct HashTable;

struct HashEntry {
	struct HashEntry *next;
	unsigned long key;
};

struct HashTable *htalloc(unsigned int nbatches);
void htfree(struct HashTable *ht);
struct HashEntry *htfind(struct HashTable *ht, unsigned long key,
	struct HashEntry **prev);
void htdel(struct HashTable *ht, unsigned long key);
void htadd(struct HashTable *ht, struct HashEntry *he, unsigned long key);
void htaddex(struct HashTable *ht, struct HashEntry *he, unsigned long key);
void htprint(struct HashTable *ht);
void htforeach(struct HashTable *ht, void (*func)(struct HashEntry *entry));

#endif
