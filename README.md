# GC - a Simple Conservative Garbage Collector

## Introduction

GC is a simple garbage collector for programs written in C. It can collect
unreferenced objects allocated the special way (by invocation of GCNEW macro).
The collector works well with circular references and allows to transmit
pointers from one thread to another (which is impossible in [Cello][1], as I
remember).

## Warning

Please, do not use the library in any serious project, this is just a toy
overflowed with potential bugs and bottlenecks.

## How to Use It?

1) Clone the repository:

	$ git clone https://github.com/richiefreedom/gc.git

2) Make the library:

	$ make

3) Read the implementation of basic examples:

	$ vim testgc
	$ vim testgc-pt

4) Make your own application, see the Makefile for building flags of examples.

## API

All the API is defined in the header file gc.h, just copy the files gc.h and
libgc.so to the directory of your own project (the files can be also installed
globally for the whole system by copying the to appropriate `/usr/lib/` and
`/usr/include/` directories).

Include the file gc.h to all the files that use GC feature:

	#include "gc.h"

Mark all collectable pointers with a special modifier `gcpointer`, just see the
example for object:

	struct task {
		char *gcpointer name;
		struct task_attributes *gcpointer attrs;
		enum task_status status;
		task_fn_t function;
		void *gcpointer private;
	};

and for stack allocation:

	void memdesc_get_addr(long address) {
		struct memdesc *gcpointer md; 

		md = memdesc_from_area(area_from_address(address));
		atomic_inc(md->ref);
	}

Allocate memory for collectable objects only by GCNEW:

	struct platform_info *gcpointer pi;

	pi = GCNEW(struct platform_info);

Create thread functions only with the macro GC_THRFUNC_DECLARE:

	GC_THRFUNC_DECLARE(background_thread, void *data)
	{
		...
	}

Call gccollect from appropriate places. By default, garbage is collected when
some thread is finished or the main function returns. But you can invoke the
collector in any places. Note that the collection algorithm is very slow, so,
call it only in places without strict time constraints.

[1]: http://libcello.org/ "Cello"
