#include "Intger64.h"
#include "stdio.h"
template<class T>
_Integer64<T>::_Integer64(int l,const T* h)
{
	low=(uint)l;
	if(h!=NULL)
	{
		high=*h;
	}
	else
	{
		if(l>=0)
			high=(T)0;
		else
			high=(T)-1;
	}
}
template<class T>
DLL _Integer64<T> operator+(const _Integer64<T>& a,const _Integer64<T>& b)
{
	T c;
	int low=((~a.low<b.low?c=1:c=0),a.low+b.low);
	T high=a.high+b.high+c;
	return _Integer64<T>(low,&high);
}
template<class T>
DLL _Integer64<T> operator-(const _Integer64<T>& a,const _Integer64<T>& b)
{
	T c;
	int low=((a.low<b.low?c=1:c=0),a.low-b.low);
	T high=a.high-b.high-c;
	return _Integer64<T>(low,&high);
}
template<class T>
DLL bool operator==(const _Integer64<T>& a,const _Integer64<T>& b)
{
	return a.low==b.low&&a.high==b.high;
}
template<class T>
DLL bool operator!=(const _Integer64<T>& a,const _Integer64<T>& b)
{
	return !(a==b);
}
template<class T>
DLL bool operator<(const _Integer64<T>& a,const _Integer64<T>& b)
{
	if(a.high!=b.high)
		return a.high<b.high;
	else
		return a.low<b.low;
}
template<class T>
DLL bool operator>(const _Integer64<T>& a,const _Integer64<T>& b)
{
	if(a.high!=b.high)
		return a.high>b.high;
	else
		return a.low>b.low;
}
DLL void __unused_int64_func__()
{
	Integer64 int64_1,int64_2;
	UInteger64 uint64_1,uint64_2;
	Integer64 sum=int64_1+int64_2,
		diff=int64_1-int64_2;
	UInteger64 usum=uint64_1+uint64_2,
		udiff=uint64_1-uint64_2;
	if(int64_1==int64_2&&int64_1!=int64_2
		&&int64_1<int64_2&&int64_1>int64_2)
	{
		printf("unuesd\n");
	}
	if(uint64_1==uint64_2&&uint64_1!=uint64_2
		&&uint64_1<uint64_2&&uint64_1>uint64_2)
	{
		printf("unuesd\n");
	}
}
