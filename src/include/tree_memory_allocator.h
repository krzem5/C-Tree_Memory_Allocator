#ifndef __TREE_MEMORY_ALLOCATOR_H__
#define __TREE_MEMORY_ALLOCATOR_H__ 1
#include <stddef.h>



void init_allocator(void);



void* allocate(size_t sz);



void deallocate(void* p);



void deinit_allocator(void);



#endif
