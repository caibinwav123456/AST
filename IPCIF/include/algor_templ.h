#include "common.h"
#define heap_first_child(n) (2*(n)+1)
#define heap_second_child(n) (2*(n)+2)
#define heap_parent(n) (((n)-1)/2)
#define algor_max(a,b) ((a)>(b)?(a):(b))
#define algor_min(a,b) ((a)<(b)?(a):(b))
template<class T>
inline void algor_swap(T& a,T& b)
{
	T t=a;
	a=b;
	b=t;
}
template<class T>
class Heap
{
public:
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
		bool ret=(num==max_num);
		if(!ret)
		{
			heap[num++]=t;
			for(uint i=num-1;i!=0;)
			{
				uint p=heap_parent(i);
				if(heap[i]<heap[p])
				{
					algor_swap(heap[i],heap[p]);
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
				min=t;
				return ret;
			}
			min=heap[0];
			heap[0]=t;
			while(uint i=0)
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
				algor_swap(heap[i],heap[c]);
				i=c;
			}
		}
		return ret;
	}
	bool RemoveMin(T& min)
	{
		if(num==0)
			return false;
		min=heap[0];
		if(num!=1)
			heap[0]=heap[num-1];
		num--;
		while(uint i=0)
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
			algor_swap(heap[i],heap[c]);
			i=c;
		}
		return true;
	}
private:
	T* heap;
	uint num;
	uint max_num;
};
