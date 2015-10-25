#ifndef __GC_H__
#define __GC_H__

extern struct HashTable *__gc_hash;

int gcinit(void);
struct GCContext *gccontalloc(unsigned long stackbottom);
void gccontfree(struct GCContext *cont);
void gcscanstack(struct GCContext *cont);
void *gcalloc(struct GCContext *cont, unsigned long size);
unsigned long *getstackboard(void);
void gccollect(void);

struct GCContext;

#define GCNEW(type) \
	gcalloc(gc, sizeof(type))

#define gcpointer volatile

#define GC_THRFUNC_DECLARE(name, arg) \
void *name(void *p) \
{ \
	void *res; \
	extern void *__gc_thrfunc_##name(arg); \
	gc = gccontalloc((unsigned long) getstackboard()); \
	res = __gc_thrfunc_##name(p); \
	gccollect(); \
	gccontfree(gc); \
	return res; \
} \
void *__gc_thrfunc_##name(arg)

int __gc_main(int argc, char *argv[]);
extern __thread struct GCContext *gc;

#define main(...) \
main(int argc, char *argv[]) \
{ \
	int ret; \
	gcinit(); \
	gc = gccontalloc((unsigned long) getstackboard()); \
	ret = __gc_main(argc, argv); \
	gccollect(); \
	gccontfree(gc); \
	return ret; \
} \
__thread struct GCContext *gc; \
int __gc_main(int argc, char *argv[])

#endif
