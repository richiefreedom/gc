/*
 * Something looking and behaving like memory garbage collector, but, really,
 * please, don't use it in any serious projects - it looks buggy, slow and
 * ugly. It's just a try to make two-three evenings little bit more
 * interesting, nothing more.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>

#include "list.h"
#include "hashtable.h"

#define GCHASHNRBATCHES 128
#define MAX_GC_CONTEXTS 64
#define SIGTHRSTOP      SIGUSR1
#define SIGTHRCONT      SIGUSR2

#define DEBUG

#ifdef DEBUG
#define dbprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define dbprintf(fmt, ...) do {} while (0)
#endif

struct HashTable *__gchash;
pthread_mutex_t __gchashmutex;
sem_t __gcstopsem;
sigset_t __gcstopsigmask;
LISTHEAD(__gcthreadlist);

/**
 * struct GCContext - per thread GC context used to keep stack bottom pointer
 * and transfer the top one during memory collection.
 *
 * @list: a field used to combime all contexts in one double-linked list
 * @stackbottom: bottom of the stack for the thread
 * @stacktop: top of the stack for the thread
 * @tid: thread ID
 */
struct GCContext {
	struct ListHead list;

	unsigned long *stackbottom;
	unsigned long *volatile stacktop;

	pthread_t tid;
};

/**
 * struct GCObject - a descriptor of collectable object.
 *
 * Each collectable object has such descriptor at the beginning, gcalloc
 * allocates more memory than user requests, so there is a place for such
 * descriptor.
 *
 * @entry: a hash entry descriptor used to store the object in a hash table
 * @size: a size of the object
 * @mark: a field used for GC mark and sweep
 */
struct GCObject {
	struct HashEntry entry;
	unsigned long size;
	unsigned long mark;
};

/**
 * getstackboard() - returns current stack pointer.
 *
 * Crazy hacky, it is not a good practice to return pointers to automatically
 * allocated data, but it is intentional action, be sure.
 *
 * Returns current stack pointer.
 */
unsigned long *getstackboard(void)
{
	unsigned long dummy;
	return &dummy;
}

/**
 * gclookupcontext() - lookup GC context by thread ID.
 * @tid: thread ID we are looking for
 *
 * Returns a pointer to GC context descriptor on success, NULL - otherwise.
 */
struct GCContext *gclookupcontext(pthread_t tid)
{
	struct ListHead *pos;
	struct GCContext *cont;

	listforeach(pos, &__gcthreadlist) {
		cont = listentry(pos, struct GCContext, list);
		if (cont->tid == tid)
			return cont;
	}

	return NULL;
}

/**
 * getobjstart() - calculate an address of object's descriptor.
 * @addr: address of the object
 *
 * Returns a pointer to GC object descriptor.
 */
static inline struct GCObject *getobjstart(unsigned long addr)
{
	return (struct GCObject *)(addr - sizeof(struct GCObject));
}

/**
 * gcstopsighandler() - signal handler for SIGTHRSTOP.
 *
 * This signal is sent when the collecting thread wants to stop some other
 * thread.
 *
 * The third argument is unused now, but it is not correct way, see the comment
 * below. The context will be used soon to get correct thread's stack top
 * pointer.
 */
static void gcstopsighandler(int sig, siginfo_t *info, void *unused)
{
	int old_errno = errno;
	struct GCContext *cont;
	pthread_t tid = pthread_self();

	cont = gclookupcontext(tid);
	if (!cont)
		goto out;

	/*
	 * Save the stacktop and let the collecting thread continue its work.
	 *
	 * WARNING:
	 * Will not work for threads with private stacks for signal handlers.
	 * Moreover, it doesn't work for x86_64 in general case.
	 */
	cont->stacktop = getstackboard();
	sem_post(&__gcstopsem);

	/* Wait SIGTHRCONT from the collecting thread..*/
	sigsuspend(&__gcstopsigmask);
out:
	errno = old_errno;
}

/**
 * gccontsighandler - handler for SIGTHRCONT.
 */
static void gccontsighandler(int sig)
{
	int old_errno = errno;
	/* Is there something to do here? */
	errno = old_errno;
}

int gcinit(void)
{
	struct sigaction sigact;

	__gchash = htalloc(GCHASHNRBATCHES);
	if (!__gchash) {
		return -1;
	}

	pthread_mutex_init(&__gchashmutex, NULL);
	sem_init(&__gcstopsem, 0, 0);

	sigact.sa_flags = SA_RESTART | SA_SIGINFO;
	sigfillset(&sigact.sa_mask);
	sigdelset(&sigact.sa_mask, SIGINT);
	sigdelset(&sigact.sa_mask, SIGQUIT);
	sigdelset(&sigact.sa_mask, SIGABRT);
	sigdelset(&sigact.sa_mask, SIGTERM);

	sigact.sa_sigaction = gcstopsighandler;
	sigaction(SIGTHRSTOP, &sigact, NULL);

	sigact.sa_flags &= ~SA_SIGINFO;
	sigact.sa_handler = gccontsighandler;
	sigaction(SIGTHRCONT, &sigact, NULL);

	sigfillset(&__gcstopsigmask);
	sigdelset(&__gcstopsigmask, SIGINT);
	sigdelset(&__gcstopsigmask, SIGQUIT);
	sigdelset(&__gcstopsigmask, SIGABRT);
	sigdelset(&__gcstopsigmask, SIGTERM);
	sigdelset(&__gcstopsigmask, SIGTHRCONT);

	return 0;
}

/**
 * gccontalloc() - create GC context descriptor.
 * @stackbottom: an address of relative stack bottom
 *
 * Returns pointer to GC context descriptor on success, NULL - otherwise.
 */
struct GCContext *gccontalloc(unsigned long stackbottom)
{
	struct GCContext *cont;

	cont = malloc(sizeof(*cont));
	if (!cont)
		return NULL;

	cont->stackbottom = (unsigned long *)stackbottom;
	cont->tid = pthread_self();

	pthread_mutex_lock(&__gchashmutex);
	listadd(&cont->list, &__gcthreadlist);
	pthread_mutex_unlock(&__gchashmutex);

	return cont;
}

/**
 * gccontfree() - GC context destructor.
 * @cont: a pointer to GC context descriptor
 */
void gccontfree(struct GCContext *cont)
{
	if (!cont)
		return;

	pthread_mutex_lock(&__gchashmutex);
	listdel(&cont->list);
	pthread_mutex_unlock(&__gchashmutex);

	free(cont);
}

/**
 * gcobjexists() - check GC object with specified address on existance.
 * @cont: a pointer to GC context descriptor
 * @addr: address at which the object is expected to be located
 *
 * Returns a pointer to hash entry found in the objects' hash table.
 */
static struct HashEntry *gcobjexists(struct GCContext *cont, unsigned long addr)
{
	struct HashEntry *prev;
	struct HashEntry *found;

	return htfind(__gchash, addr, &prev);
}

static int gcmarkreferenced(struct GCContext *cont, struct GCObject *obj)
{
	obj->mark |= 0x1;
}

static int gcmarkprocessed(struct GCContext *cont, struct GCObject *obj)
{
	obj->mark |= 0x2;
}

static int gcisprocessed(struct GCContext *cont, struct GCObject *obj)
{
	return (obj->mark & 0x2);
}

static void gccleanmark(struct HashEntry *entry)
{
	struct GCObject *obj = (struct GCObject*)entry;
	obj->mark = 0;
}

static void gcobjstat(struct HashEntry *entry)
{
	struct GCObject *obj = (struct GCObject*)entry;
	dbprintf("OBJECT {\n");
	dbprintf("\tsize: %lu\n", obj->size);
	dbprintf("\tmark: %lu\n", obj->mark);
	dbprintf("OBJECT }\n");
}

/**
 * gcscanobj() - scan some collectable object to find pointers to other
 * collectable objects.
 * @cont: a pointer to GC context
 * @obj: a pointer to collectable object descriptor
 */
static void gcscanobj(struct GCContext *cont, struct GCObject *obj)
{
	struct HashEntry *found;
	unsigned long *top;
	unsigned long *bottom;

	if (!cont)
		return;
	if (!obj)
		return;

	/* Do not handle already processed objects. */
	if (gcisprocessed(cont, obj)) {
		dbprintf("Already processed, size: %lu\n", obj->size);
		return;
	}

	gcmarkprocessed(cont, obj);

	dbprintf("Scanning the object:\n");
	dbprintf("Size: %lu\n", obj->size);

	bottom = (unsigned long *)((unsigned long)obj +
			sizeof(struct GCObject));
	top = (unsigned long *)((unsigned char *)bottom + obj->size);

	dbprintf("Top: %p, bottom: %p\n", top, bottom);
	dbprintf("Hash:\n");

#ifdef DEBUG
	htprint(__gchash);
#endif

	/*
	 * Step through the object looking for pointers to collectable objects.
	 */
	for (top--; top >= bottom; top--) {
		dbprintf("Address: %p, value: %lx, normalized: %lx\n",
				top, (unsigned long)*top,
				(unsigned long)getobjstart(*top));
		found = gcobjexists(cont,
			(unsigned long)getobjstart(*top));
		if (found) {
			dbprintf("Reference found!\n");
			gcmarkreferenced(cont, (struct GCObject*)found);
			gcscanobj(cont, (struct GCObject *)found);
		}
	}
}

/**
 * gcfreeobj() - free an object pointed by hash entry.
 * @entry: pointer to some hash entry descriptor contained in collectable
 * object descriptor
 */
static void gcfreeobj(struct HashEntry *entry)
{
	struct GCObject *obj = (struct GCObject *)entry;

	/* Skip the object. */
	if (3 == obj->mark) {
		dbprintf("Skip the object: 0x%lx\n",
				obj->entry.key);
		return;
	}

	dbprintf("Deletion of the object: 0x%lx\n",
			obj->entry.key);
	/*
	 * TODO: optimize here, it is not necessary to lookup the entry
	 * by the key, because it is already here, in our hands!
	 */
	htdel(__gchash, obj->entry.key);
	free(obj);
}

static void gcsweep(void)
{
	htforeach(__gchash, gcfreeobj);
}

/**
 * gcscanstack() - scan the stack specified by particular GC context.
 * @cont: a pointer to GC context descriptor
 */
void gcscanstack(struct GCContext *cont)
{
	struct HashEntry *found;
	unsigned long *stacktop;
	unsigned long *stackbottom;

	if (!cont)
		return;

	dbprintf("Scanning the stack:\n");

	stacktop = cont->stacktop;
	stackbottom = cont->stackbottom;

	dbprintf("Top: %p, bottom: %p\n", stacktop, stackbottom);
	dbprintf("Hash:\n");

#ifdef DEBUG
	htprint(__gchash);
#endif

	/*
	 * The stack typically grows down, go forward and check each long-sized
	 * word. We are interested in something looking like pointers to
	 * collectable objects.
	 */
	for (stacktop++; stacktop <= stackbottom; stacktop++) {
		dbprintf("Address: %p, value: %lx, normalized: %lx\n",
				stacktop, (unsigned long)*stacktop,
				(unsigned long)getobjstart(*stacktop));
		found = gcobjexists(cont,
			(unsigned long)getobjstart(*stacktop));
		if (found) {
			dbprintf("Reference found!\n");
			gcmarkreferenced(cont, (struct GCObject*)found);
			gcscanobj(cont, (struct GCObject *)found);
		}
	}
}

/**
 * gccollect() - collect all non-referenced objects.
 */
void gccollect(void)
{
	struct ListHead *pos;
	struct GCContext *cont, *ccont;
	pthread_t tid = pthread_self();

	/* Global lock. Slow. Robust. */
	pthread_mutex_lock(&__gchashmutex);

	htforeach(__gchash, gccleanmark);

	listforeach(pos, &__gcthreadlist) {
		cont = listentry(pos, struct GCContext, list);
		if (cont->tid == tid) {
			ccont = cont;
			continue;
		}

		dbprintf("Sending stop signal to %lu.\n",
			(unsigned long)cont->tid);

		/* Stop the thread. */
		pthread_kill(cont->tid, SIGTHRSTOP);
		/* Wait while it sets thread's stack top pointer. */
		sem_wait(&__gcstopsem);

		dbprintf("Stacktop is ready.\n");

		gcscanstack(cont);

		dbprintf("Sending cont signal to %lu.\n",
			(unsigned long)cont->tid);
		/* Let the thread continue execution. */
		pthread_kill(cont->tid, SIGTHRCONT);
	}

	dbprintf("Collecting current thread\n");
	ccont->stacktop = getstackboard();

	/* Scan the stack of the current thread too. */
	gcscanstack(ccont);

#ifdef DEBUG
	htforeach(__gchash, gcobjstat);
#endif

	/* Remove all non-marked objects. */
	gcsweep();

	pthread_mutex_unlock(&__gchashmutex);
}

/**
 * gcalloc() - malloc replacement for collectable objects.
 * @cont: a pointer to GC context descriptor
 * @size: requested size of the object
 */
void *gcalloc(struct GCContext *cont, unsigned long size)
{
	unsigned long fixedsize;
	struct GCObject *o;
	unsigned char *p;

	fixedsize = size + sizeof(struct GCObject);
	p = malloc(fixedsize);
	if (!p)
		return NULL;

	o = (struct GCObject *)p;
	o->size = size;
	o->mark = 0;

	pthread_mutex_lock(&__gchashmutex);
	htaddex(__gchash, (struct HashEntry *)o, (unsigned long) o);
	pthread_mutex_unlock(&__gchashmutex);

	return (p + sizeof(struct GCObject));
}
