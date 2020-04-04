// interface.cpp : Defines the exported functions for the DLL application.
//

#include "interface.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define sys_wait_sem_quit(s,q) if(q)return q;\
	sys_wait_sem(s);\
	if(q)return q
#define sys_wait_sem_quit_timeout(s,q,t) if(q)return q;\
	if(0!=sys_wait_sem(s,t))return ERR_TIMEOUT;\
	if(q)return q

struct if_init_data
{
	int n;
	uint memsize;
	volatile int quit;
};
struct if_data
{
	int n;
	uint memsize;
	void * sem;
	void * lock;
	void * notifier;
	void ** sem_array;
	void * smem;
	void * hsmem;
	uint freenode;
	uint wknode;
	if_init_data * initial;
	byte * mem;
};
enum E_SEM_TYPE
{
	esem_entry=0,
	esem_lock,
	esem_notify,
	esem_array,
	esem_max,
};
const char* sem_types[esem_max]=
{
	"entry",
	"lock",
	"notify",
	"array",
};
struct ring_node
{
	uint next;
	uint prev;
	uint buf;
	int id;
};
ring_node* get_node(void* base, uint ring)
{
	byte* ptr=(byte*)base;
	ring_node* r=(ring_node*)(ptr+ring);
	ring_node* tail=(ring_node*)(ptr+r->prev);
	if (tail!=r)
	{
		((ring_node*)(ptr+tail->next))->prev=
			tail->prev;
		((ring_node*)(ptr+tail->prev))->next=
			tail->next;
		tail->next=tail->prev=((byte*)tail)-ptr;
		return tail;
	}
	return NULL;
}
void add_node(void* base, uint ring, uint node)
{
	byte* ptr=(byte*)base;
	ring_node* r=(ring_node*)(ptr+ring);
	ring_node* head=(ring_node*)(ptr+node);
	head->next=r->next;
	head->prev=ring;
	r->next=node;
	((ring_node*)(ptr+head->next))->prev=node;
}
void get_sem_name(char* buf, char* user, char* usage, int type, int index)
{
	sprintf(buf, "%s_%s_sem_%s_%d", user, usage, sem_types[type], index);
}
void get_smem_name(char* buf, char* user, char* usage)
{
	sprintf(buf, "%s_%s_smem", user, usage);
}
uint get_smem_size(uint unit, uint n)
{
	return (n+2)*sizeof(ring_node)+n*unit+sizeof(if_init_data);
}
void init_ring(if_data* data)
{
	byte*ptr=data->mem;
	data->freenode=0;
	data->wknode=sizeof(ring_node);
	ring_node* node;
	for(int i=0;i<data->n;i++)
	{
		node=(ring_node*)(ptr+(i+2)*sizeof(ring_node));
		node->buf=(data->n+2)*sizeof(ring_node)+i*data->memsize;
		node->next=(i==data->n-1?0:(i+2+1)*sizeof(ring_node));
		node->prev=(i==0?0:(i+2-1)*sizeof(ring_node));
		node->id=i;
	}
	node=(ring_node*)ptr;
	node->buf=0;
	node->next=2*sizeof(ring_node);
	node->prev=(data->n+2-1)*sizeof(ring_node);
	node->id=-1;
	node=(ring_node*)(ptr+sizeof(ring_node));
	node->buf=0;
	node->next=sizeof(ring_node);
	node->prev=sizeof(ring_node);
	node->id=-2;
}
bool check_exist(if_initial* init)
{
	bool already_exist=true;
	char name[64];
	get_sem_name(name, init->user, init->id, esem_entry, 0);
	for(int i=0;i<10;i++)
	{
		void* h=sys_get_sem(name);
		if(!VALID(h))
		{
			already_exist=false;
			break;
		}
		else
		{
			sys_close_sem(h);
			sys_sleep(100);
		}
	}
	return already_exist;
}
DLLAPI(int) setup_if(if_initial* init, void** handle)
{
	if(check_exist(init))
		return ERR_IF_SETUP_FAILED;
	int ret=0;
	if_data* h=new if_data;
	char name[64];
	memset(h,0,sizeof(if_data));
	h->n=init->nthread;
	h->memsize=init->smem_size;
	get_sem_name(name, init->user, init->id, esem_lock, 0);
	h->lock=sys_create_sem(1, 1, name);
	if (!VALID(h->lock))
	{
		ret=ERR_IF_SETUP_FAILED;
		goto fail;
	}
	get_sem_name(name, init->user, init->id, esem_notify, 0);
	h->notifier=sys_create_sem(0, h->n, name);
	if (!VALID(h->notifier))
	{
		ret=ERR_IF_SETUP_FAILED;
		goto fail;
	}
	h->sem_array=new void *[h->n];
	memset(h->sem_array,0,h->n*sizeof(void*));
	for (int i=0;i<h->n;i++)
	{
		get_sem_name(name, init->user, init->id, esem_array, i);
		h->sem_array[i]=sys_create_sem(0, 1, name);
		if (!VALID(h->sem_array[i]))
		{
			ret=ERR_IF_SETUP_FAILED;
			goto fail;
		}
	}
	get_smem_name(name, init->user, init->id);
	h->hsmem=sys_create_smem(get_smem_size(init->smem_size,init->nthread), name);
	if (!VALID(h->hsmem))
	{
		ret=ERR_IF_SETUP_FAILED;
		goto fail;
	}
	h->smem=sys_map_smem(h->hsmem);
	if (!VALID(h->smem))
	{
		ret=ERR_IF_SETUP_FAILED;
		goto fail;
	}
	h->initial=(if_init_data*)(h->smem);
	h->mem=((byte*)(h->smem))+sizeof(if_init_data);
	h->initial->n=h->n;
	h->initial->memsize=h->memsize;
	h->initial->quit=0;
	get_sem_name(name, init->user, init->id, esem_entry, 0);
	h->sem=sys_create_sem(h->n, h->n, name);
	if (!VALID(h->sem))
	{
		ret=ERR_IF_SETUP_FAILED;
		goto fail;
	}
	init_ring(h);
	*handle=h;
	return ret;
fail:
	close_if(h);
	return ret;
}
DLLAPI(int) connect_if(if_initial* init, void** handle)
{
	int ret=0;
	if_data* h=new if_data;
	char name[64];
	memset(h,0,sizeof(if_data));
	get_sem_name(name, init->user, init->id, esem_entry, 0);
	h->sem=sys_get_sem(name);
	if (!VALID(h->sem))
	{
		ret=ERR_IF_CONN_FAILED;
		goto fail;
	}
	get_smem_name(name, init->user, init->id);
	h->hsmem=sys_get_smem(name);
	if (!VALID(h->hsmem))
	{
		ret=ERR_IF_CONN_FAILED;
		goto fail;
	}
	h->smem=sys_map_smem(h->hsmem);
	if (!VALID(h->smem))
	{
		ret=ERR_IF_CONN_FAILED;
		goto fail;
	}
	h->initial=(if_init_data*)(h->smem);
	h->mem=((byte*)(h->smem))+sizeof(if_init_data);
	h->n=h->initial->n;
	h->memsize=h->initial->memsize;
	get_sem_name(name, init->user, init->id, esem_lock, 0);
	h->lock=sys_get_sem(name);
	if (!VALID(h->lock))
	{
		ret=ERR_IF_CONN_FAILED;
		goto fail;
	}
	get_sem_name(name, init->user, init->id, esem_notify, 0);
	h->notifier=sys_get_sem(name);
	if (!VALID(h->notifier))
	{
		ret=ERR_IF_CONN_FAILED;
		goto fail;
	}
	h->sem_array=new void *[h->n];
	memset(h->sem_array,0,h->n*sizeof(void*));
	for (int i=0;i<h->n;i++)
	{
		get_sem_name(name, init->user, init->id, esem_array, i);
		h->sem_array[i]=sys_get_sem(name);
		if (!VALID(h->sem_array[i]))
		{
			ret=ERR_IF_CONN_FAILED;
			goto fail;
		}
	}
	h->freenode=0;
	h->wknode=sizeof(ring_node);
	*handle=h;
	init->nthread=h->n;
	init->smem_size=h->memsize;
	return ret;
fail:
	close_if(h);
	return ret;
}
DLLAPI(int) listen_if(void* handle, if_callback cb, void* param, uint time)
{
	if_data* data=(if_data*)handle;
	if(data->initial->quit)
		return data->initial->quit;
	sys_wait_sem_quit_timeout(data->notifier, data->initial->quit,time);
	sys_wait_sem_quit(data->lock, data->initial->quit);
	ring_node* node=get_node(data->mem, data->wknode);
	sys_signal_sem(data->lock);
	assert(node!=NULL);
	if(node==NULL)
	{
		sys_signal_sem(data->notifier);
		return ERR_IF_REQUEST_FAILED;
	}
	cb(((byte*)(data->mem))+node->buf, param, OP_PARAM|OP_RETURN);
	sys_signal_sem(data->sem_array[node->id]);
	return 0;
}
DLLAPI(int) request_if(void* handle, if_callback cb, void* param)
{
	if_data* data=(if_data*)handle;
	if(data->initial->quit)
		return data->initial->quit;
	sys_wait_sem_quit(data->sem, data->initial->quit);
	sys_wait_sem_quit(data->lock, data->initial->quit);
	ring_node* node=get_node(data->mem, data->freenode);
	sys_signal_sem(data->lock);
	assert(node!=NULL);
	if(node==NULL)
	{
		sys_signal_sem(data->sem);
		return ERR_IF_REQUEST_FAILED;
	}
	cb(((byte*)(data->mem))+node->buf, param, OP_PARAM);
	sys_wait_sem_quit(data->lock, data->initial->quit);
	add_node(data->mem, data->wknode,((byte*)node)-((byte*)(data->mem)));
	sys_signal_sem(data->lock);
	sys_signal_sem(data->notifier);
	sys_wait_sem_quit(data->sem_array[node->id], data->initial->quit);
	cb(((byte*)(data->mem))+node->buf, param, OP_RETURN);
	sys_wait_sem_quit(data->lock, data->initial->quit);
	add_node(data->mem, data->freenode, ((byte*)node)-((byte*)(data->mem)));
	sys_signal_sem(data->lock);
	sys_signal_sem(data->sem);
	return 0;
}
DLLAPI(int) stat_if(void* handle, int* n, uint* memsize)
{
	if(!VALID(handle))
		return -1;
	if_data* data=(if_data*)handle;
	if(n!=NULL)
		*n=data->n;
	if(memsize!=NULL)
		*memsize=data->memsize;
	return 0;
}
DLLAPI(void) close_if(void* handle)
{
	if_data* data=(if_data*)handle;
	if (VALID(data->lock))
		sys_close_sem(data->lock);
	if (VALID(data->notifier))
		sys_close_sem(data->notifier);
	if (data->sem_array)
	{
		for (int i=0;i<data->n;i++)
		{
			if (VALID(data->sem_array[i]))
				sys_close_sem(data->sem_array[i]);
		}
		delete[] data->sem_array;
	}
	if (VALID(data->hsmem))
		sys_close_smem(data->hsmem);
	if (VALID(data->sem))
		sys_close_sem(data->sem);
	delete data;
}
DLLAPI(void) stop_if(void* handle)
{
	if_data* data=(if_data*)handle;
	if(data->initial->quit)
		return;
	data->initial->quit=ERR_IF_REQUEST_FAILED;
	sys_sem_signal_all(data->sem);
	sys_sem_signal_all(data->lock);
	sys_sem_signal_all(data->notifier);
	for(int i=0;i<data->n;i++)
	{
		sys_sem_signal_all(data->sem_array[i]);
	}
}
/*
 *This function can only be called by the server.
 *Before calling this function, one must ensure
 *that all the listen_if function calls are returned.
 */
DLLAPI(void) reset_if(void* handle)
{
	if_data* data=(if_data*)handle;
	if(data->initial->quit)
		return;
	data->initial->quit=ERR_IF_RESET;
	sys_sem_signal_all(data->sem);
	sys_sem_signal_all(data->lock);
	sys_sem_signal_all(data->notifier);
	for(int i=0;i<data->n;i++)
	{
		sys_sem_signal_all(data->sem_array[i]);
	}
	sys_sleep(100);
	init_ring(data);
	for(int i=0;i<data->n;i++)
	{
		sys_wait_sem(data->notifier);
		sys_wait_sem(data->sem_array[i]);
	}
	data->initial->quit=0;
}
DLLAPI(int) stopped_if(void* handle)
{
	if_data* data=(if_data*)handle;
	return data->initial->quit?1:0;
}