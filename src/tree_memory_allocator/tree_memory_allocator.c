#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <tree_memory_allocator.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>



#define _ASSERT_STR_(x) #x
#define _ASSERT_STR(x) _ASSERT_STR_(x)
#define ASSERT(x) \
	do{ \
		if (!(x)){ \
			printf("File \"%s\", Line %u (%s): %s: Assertion Failed\n",__FILE__,__LINE__,__func__,_ASSERT_STR(x)); \
			raise(SIGABRT); \
		} \
	} while (0)

#define ALLOCATOR_ALIGNMENT 16

#define MAX_ALLOCATION_SIZE 0x3fffffff

#define CORRECT_ALIGNMENT(n) ASSERT(!(((uint64_t)(n))&(ALLOCATOR_ALIGNMENT-1)))

#define ALIGN(a) ((((uint64_t)(a))+ALLOCATOR_ALIGNMENT-1)&(~(ALLOCATOR_ALIGNMENT-1)))

#define FLAG_COLOR_BLACK 0
#define FLAG_COLOR_RED 1
#define FLAG_USED 2
#define FLAG_CHAIN 4
#define FLAG_NO_NEXT 8

#define RB_NODE_GET_VALUE(n) ((n)->v&0x3ffffff0)
#define RB_NODE_GET_PREV_SIZE(n) ((n)->v>>34)
#define RB_NODE_GET_COLOR(n) ((n)->v&1)
#define RB_NODE_SET_COLOR(n,c) ((n)->v=((n)->v&0xfffffffffffffffeull)|(c))

#define ALLOCATION_PAGE_COUNT 16



typedef struct __PAGE_HEADER{
	void* n;
	uint64_t sz;
} page_header_t;



typedef struct __RB_NODE{
	uint64_t v;
	struct __RB_NODE* p;
	struct __RB_NODE* l;
	struct __RB_NODE* r;
	struct __RB_NODE* s;
} rb_node_t;



typedef struct __USED_RB_NODE{
	uint64_t v;
} used_rb_node_t;



static rb_node_t _allocator_nil_node;
static rb_node_t* _allocator_root=NULL;
static uint64_t _allocator_pg_sz=0;
static page_header_t* _allocator_pg=NULL;



static void _add_node(rb_node_t* o){
	ASSERT(!(o->v&7));
	rb_node_t* x=_allocator_root;
	rb_node_t* y=NULL;
	while (x!=&_allocator_nil_node){
		y=x;
		if (RB_NODE_GET_VALUE(x)>RB_NODE_GET_VALUE(o)){
			x=x->l;
		}
		else if (RB_NODE_GET_VALUE(x)<RB_NODE_GET_VALUE(o)){
			x=x->r;
		}
		else{
			if (x->v&FLAG_CHAIN){
				o->s=x->s;
				o->s->p=o;
			}
			else{
				x->v|=FLAG_CHAIN;
				o->s=NULL;
			}
			o->v|=FLAG_CHAIN;
			o->p=x;
			o->r=NULL;
			x->s=o;
			return;
		}
	}
	o->v|=FLAG_COLOR_RED;
	o->p=y;
	o->l=&_allocator_nil_node;
	o->r=&_allocator_nil_node;
	if (!y){
		_allocator_root=o;
		RB_NODE_SET_COLOR(o,FLAG_COLOR_BLACK);
		return;
	}
	if (RB_NODE_GET_VALUE(o)<RB_NODE_GET_VALUE(y)){
		y->l=o;
	}
	else{
		y->r=o;
	}
	if (o->p->p){
		rb_node_t* n=o;
		while (RB_NODE_GET_COLOR(n->p)==FLAG_COLOR_RED){
			if (n->p==n->p->p->r){
				rb_node_t* u=n->p->p->l;
				if (RB_NODE_GET_COLOR(u)==FLAG_COLOR_BLACK){
					if (n==n->p->l){
						n=n->p;
						rb_node_t* x=n->l;
						n->l=x->r;
						x->p=n->p;
						if (x->r!=&_allocator_nil_node){
							x->r->p=n;
						}
						if (!n->p){
							_allocator_root=x;
						}
						else if (n==n->p->r){
							n->p->r=x;
						}
						else{
							n->p->l=x;
						}
						n->p=x;
						x->r=n;
					}
					RB_NODE_SET_COLOR(n->p,FLAG_COLOR_BLACK);
					x=n->p->p;
					RB_NODE_SET_COLOR(x,FLAG_COLOR_RED);
					rb_node_t* y=x->r;
					x->r=y->l;
					y->p=x->p;
					if (y->l!=&_allocator_nil_node){
						y->l->p=x;
					}
					if (!x->p){
						_allocator_root=y;
					}
					else if (x==x->p->l){
						x->p->l=y;
					}
					else{
						x->p->r=y;
					}
					x->p=y;
					y->l=x;
				}
				else{
					RB_NODE_SET_COLOR(u,FLAG_COLOR_BLACK);
					RB_NODE_SET_COLOR(n->p,FLAG_COLOR_BLACK);
					n=n->p->p;
					RB_NODE_SET_COLOR(n,FLAG_COLOR_RED);
				}
			}
			else{
				rb_node_t* u=n->p->p->r;
				if (RB_NODE_GET_COLOR(u)==FLAG_COLOR_BLACK){
					if (n==n->p->r){
						n=n->p;
						rb_node_t* x=n->r;
						n->r=x->l;
						x->p=n->p;
						if (x->l!=&_allocator_nil_node){
							x->l->p=n;
						}
						if (!n->p){
							_allocator_root=x;
						}
						else if (n==n->p->l){
							n->p->l=x;
						}
						else{
							n->p->r=x;
						}
						n->p=x;
						x->l=n;
					}
					RB_NODE_SET_COLOR(n->p,FLAG_COLOR_BLACK);
					x=n->p->p;
					RB_NODE_SET_COLOR(x,FLAG_COLOR_RED);
					rb_node_t* y=x->l;
					x->l=y->r;
					y->p=x->p;
					if (y->r!=&_allocator_nil_node){
						y->r->p=x;
					}
					if (!x->p){
						_allocator_root=y;
					}
					else if (x==x->p->r){
						x->p->r=y;
					}
					else{
						x->p->l=y;
					}
					x->p=y;
					y->r=x;
				}
				else{
					RB_NODE_SET_COLOR(u,FLAG_COLOR_BLACK);
					RB_NODE_SET_COLOR(n->p,FLAG_COLOR_BLACK);
					n=n->p->p;
					RB_NODE_SET_COLOR(n,FLAG_COLOR_RED);
				}
			}
			if (n==_allocator_root){
				break;
			}
		}
		RB_NODE_SET_COLOR(_allocator_root,FLAG_COLOR_BLACK);
	}
}



static void _delete_node(rb_node_t* n){
	if (n->v&FLAG_CHAIN){
		if (n->r){
			rb_node_t* nn=n->s;
			if (n->p){
				if (n==n->p->r){
					n->p->r=nn;
				}
				else{
					n->p->l=nn;
				}
			}
			n->l->p=nn;
			n->r->p=nn;
			nn->p=n->p;
			nn->l=n->l;
			nn->r=n->r;
			if (!nn->s){
				nn->v&=~FLAG_CHAIN;
			}
		}
		else{
			if (!n->s){
				if (n->p->r){
					n->p->v&=~FLAG_CHAIN;
				}
				else{
					n->p->s=NULL;
				}
			}
			else{
				n->p->s=n->s;
				n->s->p=n->p;
			}
		}
		return;
	}
	uint8_t cl=RB_NODE_GET_COLOR(n);
	rb_node_t* x=NULL;
	if (n->l==&_allocator_nil_node){
		if (!n->p){
			_allocator_root=n->r;
		}
		else if (n==n->p->l){
			n->p->l=n->r;
		}
		else{
			n->p->r=n->r;
		}
		n->r->p=n->p;
		x=n->r;
	}
	else if (n->r==&_allocator_nil_node){
		if (!n->p){
			_allocator_root=n->l;
		}
		else if (n==n->p->l){
			n->p->l=n->l;
		}
		else{
			n->p->r=n->l;
		}
		n->l->p=n->p;
		x=n->l;
	}
	else{
		rb_node_t* y=n->r;
		while (y->l!=&_allocator_nil_node){
			y=y->l;
		}
		cl=RB_NODE_GET_COLOR(y);
		x=y->r;
		if (y->p==n){
			x->p=y;
		}
		else{
			if (!y->p){
				_allocator_root=y->r;
			}
			else if (y==y->p->l){
				y->p->l=y->r;
			}
			else{
				y->p->r=y->r;
			}
			y->r->p=y->p;
			y->r=n->r;
			y->r->p=y;
		}
		if (!n->p){
			_allocator_root=y;
		}
		else if (n==n->p->l){
			n->p->l=y;
		}
		else{
			n->p->r=y;
		}
		y->p=n->p;
		y->l=n->l;
		y->l->p=y;
		RB_NODE_SET_COLOR(y,RB_NODE_GET_COLOR(n));
	}
	if (cl==FLAG_COLOR_RED){
		return;
	}
	while (x!=_allocator_root&&RB_NODE_GET_COLOR(x)==FLAG_COLOR_BLACK){
		if (x==x->p->l){
			rb_node_t*y=x->p->r;
			if (RB_NODE_GET_COLOR(y)==FLAG_COLOR_RED){
				RB_NODE_SET_COLOR(y,FLAG_COLOR_BLACK);
				rb_node_t* z=x->p;
				RB_NODE_SET_COLOR(z,FLAG_COLOR_RED);
				y=z->r;
				z->r=y->l;
				y->p=z->p;
				if (y->l!=&_allocator_nil_node){
					y->l->p=z;
				}
				if (!z->p){
					_allocator_root=y;
				}
				else if (z==z->p->l){
					z->p->l=y;
				}
				else{
					z->p->r=y;
				}
				z->p=y;
				y->l=z;
			}
			if (RB_NODE_GET_COLOR(y->l)==FLAG_COLOR_BLACK&&RB_NODE_GET_COLOR(y->r)==FLAG_COLOR_BLACK){
				RB_NODE_SET_COLOR(y,FLAG_COLOR_RED);
				x=x->p;
			}
			else{
				if (RB_NODE_GET_COLOR(y->r)==FLAG_COLOR_BLACK){
					RB_NODE_SET_COLOR(y->l,FLAG_COLOR_BLACK);
					RB_NODE_SET_COLOR(y,FLAG_COLOR_RED);
					rb_node_t* z=y->l;
					y->l=z->r;
					z->p=y->p;
					if (z->r!=&_allocator_nil_node){
						z->r->p=y;
					}
					if (!y->p){
						_allocator_root=z;
					}
					else if (y==y->p->r){
						y->p->r=z;
					}
					else{
						y->p->l=z;
					}
					y->p=z;
					z->r=y;
					y=x->p->r;
				}
				RB_NODE_SET_COLOR(y,RB_NODE_GET_COLOR(x->p));
				RB_NODE_SET_COLOR(x->p,FLAG_COLOR_BLACK);
				RB_NODE_SET_COLOR(y->r,FLAG_COLOR_BLACK);
				rb_node_t* z=x->p;
				x=_allocator_root;
				rb_node_t* w=z->r;
				z->r=w->l;
				w->p=z->p;
				if (w->l!=&_allocator_nil_node){
					w->l->p=z;
				}
				if (!z->p){
					_allocator_root=w;
				}
				else if (z==z->p->l){
					z->p->l=w;
				}
				else{
					z->p->r=w;
				}
				z->p=w;
				w->l=z;
			}
		}
		else{
			rb_node_t* y=x->p->l;
			if (RB_NODE_GET_COLOR(y)==FLAG_COLOR_RED){
				RB_NODE_SET_COLOR(y,FLAG_COLOR_BLACK);
				rb_node_t* z=x->p;
				RB_NODE_SET_COLOR(z,FLAG_COLOR_RED);
				y=z->l;
				z->l=y->r;
				y->p=z->p;
				if (y->r!=&_allocator_nil_node){
					y->r->p=z;
				}
				if (!z->p){
					_allocator_root=y;
				}
				else if (z==z->p->r){
					z->p->r=y;
				}
				else{
					z->p->l=y;
				}
				z->p=y;
				y->r=z;
			}
			if (RB_NODE_GET_COLOR(y->l)==FLAG_COLOR_BLACK&&RB_NODE_GET_COLOR(y->r)==FLAG_COLOR_BLACK){
				RB_NODE_SET_COLOR(y,FLAG_COLOR_RED);
				x=x->p;
			}
			else{
				if (RB_NODE_GET_COLOR(y->l)==FLAG_COLOR_BLACK){
					RB_NODE_SET_COLOR(y->r,FLAG_COLOR_BLACK);
					RB_NODE_SET_COLOR(y,FLAG_COLOR_RED);
					rb_node_t* z=y->r;
					y->r=z->l;
					z->p=y->p;
					if (z->l!=&_allocator_nil_node){
						z->l->p=y;
					}
					if (!y->p){
						_allocator_root=z;
					}
					else if (y==y->p->l){
						y->p->l=z;
					}
					else{
						y->p->r=z;
					}
					y->p=z;
					z->l=y;
					y=x->p->l;
				}
				RB_NODE_SET_COLOR(y,RB_NODE_GET_COLOR(x->p));
				RB_NODE_SET_COLOR(x->p,FLAG_COLOR_BLACK);
				RB_NODE_SET_COLOR(y->l,FLAG_COLOR_BLACK);
				rb_node_t* z=x->p;
				x=_allocator_root;
				rb_node_t* w=z->l;
				z->l=w->r;
				w->p=z->p;
				if (w->r!=&_allocator_nil_node){
					w->r->p=z;
				}
				if (!z->p){
					_allocator_root=w;
				}
				else if (z==z->p->r){
					z->p->r=w;
				}
				else{
					z->p->l=w;
				}
				z->p=w;
				w->r=z;
			}
		}
	}
	RB_NODE_SET_COLOR(x,FLAG_COLOR_BLACK);
}



void init_allocator(void){
	ASSERT(!_allocator_root);
	ASSERT(!_allocator_pg);
	ASSERT(!(((uint64_t)(&_allocator_nil_node))&15));
	_allocator_nil_node.p=NULL;
	_allocator_nil_node.l=&_allocator_nil_node;
	_allocator_nil_node.r=&_allocator_nil_node;
	_allocator_nil_node.v=FLAG_COLOR_BLACK;
	_allocator_root=&_allocator_nil_node;
#ifdef _MSC_VER
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	_allocator_pg_sz=si.dwPageSize;
#else
	_allocator_pg_sz=sysconf(_SC_PAGESIZE);
#endif
	ASSERT(!(_allocator_pg_sz&(_allocator_pg_sz-1)));
}



void* allocate(size_t sz){
	sz+=sizeof(used_rb_node_t);
	if (sz<sizeof(rb_node_t)){
		sz=sizeof(rb_node_t);
	}
	sz=ALIGN(sz);
	ASSERT(!(sz&15));
	ASSERT(sz<=MAX_ALLOCATION_SIZE);
	rb_node_t* n=NULL;
	if (_allocator_pg){
		n=_allocator_root;
		rb_node_t* p=NULL;
		do{
			if (sz>RB_NODE_GET_VALUE(n)){
				n=n->l;
			}
			else if (sz==RB_NODE_GET_VALUE(n)){
				p=n;
				break;
			}
			else{
				p=n;
				n=n->r;
			}
		} while (n!=&_allocator_nil_node);
		n=p;
		if (n&&(n->v&FLAG_CHAIN)){
			p=n->s;
			ASSERT(p);
			if (p->s){
				n->s=p->s;
				p->s->p=n;
			}
			else{
				n->v&=~FLAG_CHAIN;
			}
			n=p;
			n->r=NULL;
		}
	}
	if (!n){
		uint64_t pg_sz=(sz+sizeof(page_header_t)+_allocator_pg_sz-1)&(~(_allocator_pg_sz-1));
		if (pg_sz<_allocator_pg_sz*ALLOCATION_PAGE_COUNT){
			pg_sz=_allocator_pg_sz*ALLOCATION_PAGE_COUNT;
		}
#ifdef _MSC_VER
		void* pg=VirtualAlloc(NULL,pg_sz,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
#else
		void* pg=mmap(NULL,pg_sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#endif
		page_header_t* h=(page_header_t*)pg;
		h->n=_allocator_pg;
		h->sz=pg_sz;
		_allocator_pg=pg;
		n=(rb_node_t*)(((uint64_t)pg)+sizeof(page_header_t));
		n->v=pg_sz-sizeof(page_header_t);
		ASSERT(!(n->v&15)&&n->v<=MAX_ALLOCATION_SIZE);
		n->v|=FLAG_NO_NEXT;
		n->r=NULL;
	}
	ASSERT(RB_NODE_GET_VALUE(n)>=sz);
	if (RB_NODE_GET_VALUE(n)-sz>=sizeof(rb_node_t)+ALLOCATOR_ALIGNMENT){
		rb_node_t* nn=(rb_node_t*)(((uint64_t)n)+sz);
		nn->v=(sz<<34)|(RB_NODE_GET_VALUE(n)-sz)|(n->v&FLAG_NO_NEXT);
		n->v=(n->v&0xfffffffc00000000ull)|sz|FLAG_USED;
		ASSERT(!(n->v&FLAG_NO_NEXT));
		if (n->r){
			_delete_node(n);
		}
		_add_node(nn);
	}
	else{
		if (n->r){
			_delete_node(n);
		}
		n->v=(n->v&(~((uint64_t)FLAG_COLOR_RED)))|FLAG_USED;
	}
	return (void*)(((uint64_t)n)+sizeof(used_rb_node_t));
}



void deallocate(void* p){
	rb_node_t* n=(rb_node_t*)(((uint64_t)p)-sizeof(used_rb_node_t));
	ASSERT(n->v&FLAG_USED);
	n->v&=~(FLAG_CHAIN|FLAG_USED|FLAG_COLOR_RED);
	_add_node(n);
}



void deinit_allocator(void){
	_allocator_root=NULL;
	page_header_t* pg=_allocator_pg;
	while (pg){
		page_header_t* n=pg->n;
#ifdef _MSC_VER
		VirtualFree(pg,0,MEM_RELEASE);
#else
		munmap(pg,pg->sz);
#endif
		pg=n;
	}
	_allocator_pg=NULL;
}
