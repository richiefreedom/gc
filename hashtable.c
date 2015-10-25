/*
 * Simple and idiotic imlementation of a container named "hash table". Seems to
 * be written in delirium. But it is just an experiment and I need some
 * implementation to tesm ideas in my head, so do not judge strictly..
 *
 * The coding style was chosen intentionally, with this style similar to old
 * Unix source codes I feel that I'm smart. :).
 */

#include <stdlib.h>
#include <stdio.h>

#include "hashtable.h"

struct HashTable {
	unsigned int nbatches;
	struct HashEntry **head;
};

/**
 * batchfree() - iteratively remove all elements from the batch (list).
 * @head: a pointer to the list head
 */
static void batchfree(struct HashEntry *head)
{
	if (!head) {
		return;
	}

	while (head) {
		struct HashEntry *next = head->next;

		free(head);
		head = next;
	}
}

/**
 * htalloc() - make a hash table object with 'nbatches' batches.
 * @nbatches: a number of batches (entries in list head array) for the new hash
 *            table.
 *
 * Returns pointer to a newly allocated hash table object on success, NULL -
 * otherwise.
 */
struct HashTable *htalloc(unsigned int nbatches)
{
	struct HashTable *ht;

	ht = malloc(sizeof(*ht));
	if (!ht) {
		return NULL;
	}

	ht->nbatches = nbatches;

	ht->head = calloc(nbatches, sizeof(*(ht->head)));
	if (!ht->head) {
		free(ht);
		return NULL;
	}

	{
		unsigned int i;
		for (i = 0; i < nbatches; i++) {
			ht->head[i] = NULL;
		}
	}

	return ht;
}

/**
 * htfree() - something looking like a destructor.
 * @ht: a pointer to some hash table object we've decided to delete.
 */
void htfree(struct HashTable *ht)
{
	if (!ht) {
		return;
	}

	{
		unsigned int i, nbatches = ht->nbatches;
		for (i = 0; i < nbatches; i++) {
			batchfree(ht->head[i]);
		}
	}

	free(ht);
}

/**
 * htfunc() - simplest possible hash function.
 * @ht: a pointer to hash table object (necessary to obtain nbatches)
 * @key: a key which will be mapped to the interval [0, nbatches)
 *
 * Returns a result of hash function application - a number between 0 and
 * nbatches, including the first and excepting the second one.
 */
static unsigned int htfunc(struct HashTable *ht, unsigned long key)
{
	return key % ht->nbatches;
}

/**
 * batchadd() - add a new element to a batch.
 * @head: a pointer to pointer to hash entry after which the new element should
 *        be added.
 * @elem: a pointer to an element which we want to add to the batch.
 */
static void batchadd(struct HashEntry **head, struct HashEntry *elem)
{
	elem->next = *head;
	*head = elem;
}

/**
 * batchfind() - find an element in a batch specified by a list head.
 * @head: a pointer to a batch head (first element in the list)
 * @key: a key which we are looking for
 * @prev: a pointer to pointer to previous hash entry - necessary for remove
 *        easy object deletion due to particularity of a single-linked list
 *
 * Returns a pointer to hash entry on success, NULL - otherwise. The function
 * also modifies a pointer to the previous entry 'prev'. This pointer is used
 * to delete elements. If nothing is found or the first element of the batch
 * conteins necessary key, *prev is equal to NULL.
 */
static struct HashEntry *batchfind(struct HashEntry *head, unsigned long key,
	struct HashEntry **prev)
{
	*prev = NULL;

	while (head) {
		if (head->key == key) {
			return head;
		}

		*prev = head;
		head = head->next;
	}

	return NULL;
}

/**
 * htfind() - find an element in a hash table by a key.
 * @ht:   a pointer to a hash table object
 * @key:  a key we are looking for
 * @prev: a pointer to pointer to previous hash entry, see the function above
 *
 * Returns a pointer to hash entry on success, NULL - otherwise. *prev
 * semantics are analoguous to batchfind.
 */
struct HashEntry *htfind(struct HashTable *ht, unsigned long key,
	struct HashEntry **prev)
{
	struct HashEntry *head;

	if (!ht) {
		return NULL;
	}

	head = ht->head[htfunc(ht, key)];

	return batchfind(head, key, prev);
}

/**
 * batchdel() - remove an element from a batch.
 * @head: a pointer to pointer to a list head
 * @prev: a pointer to a previous entry
 * @elem: a pointer to an entry we want to remove
 */
static void batchdel(struct HashEntry **head, struct HashEntry *prev,
		struct HashEntry *elem)
{
	if (prev)
		prev->next = elem->next;
	else
		*head = elem->next;
}

/**
 * htdel() - delete an element from hash table by a key.
 * @ht:  a pointer to a hash table object
 * @key: a key of the element we want to remove
 */
void htdel(struct HashTable *ht, unsigned long key)
{
	struct HashEntry *found;
	struct HashEntry *prev;
	struct HashEntry **head;

	if (!ht) {
		return;
	}

	found = htfind(ht, key, &prev);
	if (!found) {
		fprintf(stderr, "The entry is not found.");
		return;
	}

	head = &ht->head[htfunc(ht, key)];
	batchdel(head, prev, found);
}

/**
 * htadd() - add a new entry to a hash table.
 * @ht:  a pointer to some hash table
 * @he:  a pointer to an entry we want to add
 * @key: a key of the entry
 */
void htadd(struct HashTable *ht, struct HashEntry *he, unsigned long key)
{
	struct HashEntry **head;

	if (!ht) {
		return;
	}

	head = &ht->head[htfunc(ht, key)];
	he->key = key;
	batchadd(head, he);
}

/** Exclusive version of htadd. */
void htaddex(struct HashTable *ht, struct HashEntry *he, unsigned long key)
{
	struct HashEntry *found;
	struct HashEntry *prev;

	found = htfind(ht, key, &prev);
	if (!found)
		htadd(ht, he, key);
}

/**
 * batchprint() - print entries of a specified batch.
 * @head: a pointer to a first element of the batch
 */
void batchprint(struct HashEntry *head)
{
	while (head) {
		printf("(key:%lx) -> ", head->key);

		head = head->next;
	}

	printf("NULL\n");
}

/**
 * htprint() - print contents of a hash table.
 * @ht: a pointer to a hash table object
 */
void htprint(struct HashTable *ht)
{
	if (!ht)
		return;

	printf("Hashtable {\n");

	{
		struct HashEntry *head;
		unsigned int i;

		for (i = 0; i < ht->nbatches; i++) {
			head = ht->head[i];
			if (head) {
				printf("\tbatch[%u]: ", i);
				batchprint(head);
			}
		}
	}

	printf("}\n");
}

/**
 * batchforeach() - a kind of MAP call for a batch.
 * @head: a pointer to a first entry in the batch
 * @func: a pointer to a function dealing with hash entries
 */
static void batchforeach(struct HashEntry *head,
		void (*func)(struct HashEntry *entry))
{
	struct HashEntry *next;

	while (head) {
		next = head->next;
		func(head);
		head = next;
	}
}

/**
 * htforeach() - execute some function for each hash element.
 * @ht: a pointer to a hash table
 * @func: a pointer to a function dealing with hash entries
 */
void htforeach(struct HashTable *ht, void (*func)(struct HashEntry *entry))
{
	if (!ht)
		return;
	{
		struct HashEntry *head;
		unsigned int i;

		for (i = 0; i < ht->nbatches; i++) {
			head = ht->head[i];
			if (head) {
				batchforeach(head, func);
			}
		}
	}
}
