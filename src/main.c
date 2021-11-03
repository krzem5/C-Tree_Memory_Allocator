#include <tree_memory_allocator.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>



#define ALLOCATION_COUNT (1<<14)
#define MAX_ALLOCATION_SIZE (1<<13)



typedef struct __BLOCK{
	uint8_t* p;
	uint32_t sz;
} block_t;



int main(int argc,const char** argv){
	printf("Seed: %u\n",(unsigned int)time(NULL));
	init_allocator();
	srand(1635868628/*(unsigned int)time(NULL)*/);
	block_t bl[ALLOCATION_COUNT];
	for (uint32_t i=0;i<ALLOCATION_COUNT;i++){
		bl[i].sz=((uint64_t)rand())*(MAX_ALLOCATION_SIZE-1)/RAND_MAX+1;
		bl[i].p=allocate(bl[i].sz);
		for (uint32_t j=0;j<bl[i].sz;j++){
			*(bl[i].p+j)=0xcc;
		}
	}
	for (uint32_t i=0;i<(ALLOCATION_COUNT>>1);i++){
		uint32_t j=i+rand()/(RAND_MAX/(ALLOCATION_COUNT-i)+1);
		deallocate(bl[j].p);
		bl[j]=bl[i];
	}
	for (uint32_t i=0;i<(ALLOCATION_COUNT>>1);i++){
		bl[i].sz=((uint64_t)rand())*(MAX_ALLOCATION_SIZE-1)/RAND_MAX+1;
		bl[i].p=allocate(bl[i].sz);
		for (uint32_t j=0;j<bl[i].sz;j++){
			*(bl[i].p+j)=0xcc;
		}
	}
	for (uint32_t i=0;i<ALLOCATION_COUNT-1;i++){
		uint32_t j=i+rand()/(RAND_MAX/(ALLOCATION_COUNT-i)+1);
		deallocate(bl[j].p);
		bl[j]=bl[i];
	}
	deallocate(bl[ALLOCATION_COUNT-1].p);
	deinit_allocator();
	printf("End!\n");
	return 0;
}
