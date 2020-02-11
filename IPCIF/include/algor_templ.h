#ifndef _ALGOR_TEMPL_H_
#define _ALGOR_TEMPL_H_
#include "common.h"
#include <assert.h>
#define heap_first_child(n) (2*(n)+1)
#define heap_second_child(n) (2*(n)+2)
#define heap_parent(n) (((n)-1)/2)
#define algor_max(a,b) ((a)>(b)?(a):(b))
#define algor_min(a,b) ((a)<(b)?(a):(b))
template<class T>
class normal_assign
{
public:
	T& operator()(T& a,T& b,uint index)
	{
		a=b;
		return a;
	}
};
template<class T>
inline void algor_swap(T& a,T& b)
{
	T t=a;
	a=b;
	b=t;
}
template<class T>
class nswap
{
public:
	void operator()(T& a,T& b)
	{
		algor_swap(a,b);
	}
};
template<class T,class assign=normal_assign<T>,class cswap=nswap<T>>
class Heap
{
public:
	class iterator
	{
		friend class Heap<T,assign,cswap>;
		uint i;
		uint num;
		T* ptr;
		iterator(uint n,T* p):i(0),num(n),ptr(p)
		{
		}
	public:
		void operator++(int)
		{
			i++;
		}
		operator bool()
		{
			return i<num;
		}
		const T& operator*()
		{
			return ptr[i];
		}
		const T* operator->()
		{
			return ptr+i;
		}
	};
	Heap(uint n)
	{
		max_num=(n<1?1:n);
		num=0;
		heap=new T[max_num];
	}
	~Heap()
	{
		if(heap!=NULL)
			delete[] heap;
	}
	bool Add(T t,T& min)
	{
		assign local_assign;
		cswap c_swap;
		bool ret=(num==max_num);
		if(!ret)
		{
			local_assign(heap[num],t,num);
			num++;
			for(uint i=num-1;i!=0;)
			{
				uint p=heap_parent(i);
				if(heap[i]<heap[p])
				{
					c_swap(heap[i],heap[p]);
					i=p;
				}
				else
					break;
			}
		}
		else
		{
			if(t<=heap[0])
			{
				local_assign(min,t,0);
				return ret;
			}
			local_assign(min,heap[0],0);
			local_assign(heap[0],t,0);
			for(uint i=0;;)
			{
				uint c0=heap_first_child(i);
				uint c1=heap_second_child(i);
				uint c;
				if(c0>=max_num)
					break;
				else if(c1>=max_num)
				{
					if(heap[c0]<heap[i])
						c=c0;
					else
						break;
				}
				else
				{
					if(heap[i]<=heap[c0]&&heap[i]<=heap[c1])
						break;
					if(heap[c0]<=heap[c1])
						c=c0;
					else
						c=c1;
				}
				c_swap(heap[i],heap[c]);
				i=c;
			}
		}
		return ret;
	}
	bool RemoveMin(T& min,uint order=0)
	{
		assign local_assign;
		cswap c_swap;
		if(order>=num)
			return false;
		local_assign(min,heap[order],0);
		if(order<num-1)
			local_assign(heap[order],heap[num-1],order);
		num--;
		for(uint i=order;;)
		{
			uint c0=heap_first_child(i);
			uint c1=heap_second_child(i);
			uint c;
			if(c0>=num)
				break;
			else if(c1>=num)
			{
				if(heap[c0]<heap[i])
					c=c0;
				else
					break;
			}
			else
			{
				if(heap[i]<=heap[c0]&&heap[i]<=heap[c1])
					break;
				if(heap[c0]<=heap[c1])
					c=c0;
				else
					c=c1;
			}
			c_swap(heap[i],heap[c]);
			i=c;
		}
		return true;
	}
	iterator BeginIterate()
	{
		return iterator(num,heap);
	}
private:
	T* heap;
	uint num;
	uint max_num;
};
template<class T>
class BiRingNode
{
	BiRingNode<T>* prev;
	BiRingNode<T>* next;
public:
	T t;
	BiRingNode()
	{
		next=prev=this;
	}
	BiRingNode<T>* GetNext()
	{
		return next;
	}
	BiRingNode<T>* GetPrev()
	{
		return prev;
	}
	void AttachAfter(BiRingNode<T>* node)
	{
		next=node->next;
		prev=node;
		node->next=this;
		next->prev=this;
	}
	void Detach()
	{
		if(next==this)
		{
			assert(prev==this);
			return;
		}
		next->prev=prev;
		prev->next=next;
		next=prev=this;
	}
};
template<class T>
class BiRing : public BiRingNode<T>
{
public:
	class iterator
	{
		friend class BiRing<T>;
		BiRingNode<T>* node;
		BiRingNode<T>* ring;
		iterator(BiRingNode<T>* _node,BiRingNode<T>* _ring)
		{
			node=_node;
			ring=_ring;
		}
	public:
		void operator++(int)
		{
			node=node->GetNext();
		}
		void operator--(int)
		{
			node=node->GetPrev();
		}
		operator bool()
		{
			return node!=ring;
		}
		BiRingNode<T>& operator*()
		{
			return *node;
		}
		BiRingNode<T>* operator->()
		{
			return node;
		}
	};
	virtual ~BiRing(){}
	void AddNodeToBegin(BiRingNode<T>* node)
	{
		node->AttachAfter(this);
	}
	BiRingNode<T>* GetNodeFromTail()
	{
		if(GetNext()==this)
		{
			assert(GetPrev()==this);
			return NULL;
		}
		BiRingNode<T>* ret=GetPrev();
		ret->Detach();
		return ret;
	}
	iterator BeginIterate()
	{
		return iterator(GetNext(),this);
	}
	iterator BeginReverseIterate()
	{
		return iterator(GetPrev(),this);
	}
};
#endif
