/*
 * Basic double-linked list library. Borrowed fro the Linux kernel source code.
 * Some things unnecessary for the goals of the project had been removed.
 */

#ifndef __LIST_H__
#define __LIST_H__

struct ListHead {
	struct ListHead *next;
	struct ListHead *prev;
};

#define LISTPOISON1 ((void *) 0xDEADBEEF)
#define LISTPOISON2 ((void *) 0xDEADBABA)

#define LISTHEADINIT(name) { &(name), &(name) }

#define LISTHEAD(name) \
	struct ListHead name = LISTHEADINIT(name)

static inline void INITLISTHEAD(struct ListHead *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __listadd(struct ListHead *new, struct ListHead *prev,
			      struct ListHead *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void listadd(struct ListHead *new, struct ListHead *head)
{
	__listadd(new, head, head->next);
}

static inline void listaddtail(struct ListHead *new, struct ListHead *head)
{
	__listadd(new, head->prev, head);
}

static inline void __listdel(struct ListHead *prev, struct ListHead *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void listdel(struct ListHead *entry)
{
	__listdel(entry->prev, entry->next);
	entry->next = LISTPOISON1;
	entry->prev = LISTPOISON2;
}

static inline int listempty(const struct ListHead *head)
{
	return head->next == head;
}

#define containerof(ptr, type, member) \
	(type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member))

#define listentry(ptr, type, member) \
	containerof(ptr, type, member)

#define listforeach(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
#endif
