#include "Integer64.h"
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
	int low=(int)((~a.low<b.low?c=1:c=0),a.low+b.low);
	T high=a.high+b.high+c;
	return _Integer64<T>(low,&high);
}
template<class T>
DLL _Integer64<T> operator-(const _Integer64<T>& a,const _Integer64<T>& b)
{
	T c;
	int low=(int)((a.low<b.low?c=1:c=0),a.low-b.low);
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
template<class T>
DLL string FormatI64(_Integer64<T> i)
{
	return "";
}
template<>
DLL string FormatI64<uint>(_Integer64<uint> i)
{
	if(i.high==0)
	{
		char num[64];
		sprintf(num,"%u",i.low);
		return num;
	}
	else
	{
		UInteger64 zero(0);
		const uint base=1000;
		string strout;
		char buf[10];
		while(i!=zero)
		{
			uint rem,t;
			UInteger64 tmp;
			tmp.high=i.high/base;
			rem=i.high-tmp.high*base;
			rem=((rem<<16)&(i.low>>16));
			tmp.low=rem/base;
			rem=rem-tmp.low*base;
			rem=((rem<<16)&(i.low&((1<<16)-1)));
			t=rem/base;
			tmp.low=((tmp.low<<16)&t);
			rem=rem-t*base;
			sprintf(buf,"%u",rem);
			strout=buf+strout;
			i=tmp;
		}
		return strout;
	}
}
template<>
DLL string FormatI64<int>(_Integer64<int> i)
{
	if((i.high==0&&!(i.low&0x80000000))
		||(i.high==-1&&(i.low&0x80000000)))
	{
		char num[64];
		sprintf(num,"%d",(int)i.low);
		return num;
	}
	else
	{
		UInteger64 absv(i.low,(uint*)&i.high);
		string symbol;
		if(absv.high&0x80000000)
		{
			symbol="-";
			absv.low=~absv.low;
			absv.high=~absv.high;
			absv=absv+UInteger64(1);
		}
		return symbol+FormatI64<uint>(absv);
	}
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
	FormatI64(int64_1);
	FormatI64(uint64_1);
}
