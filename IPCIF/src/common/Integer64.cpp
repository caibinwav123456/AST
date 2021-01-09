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
DLL _Integer64<T> operator-(const _Integer64<T>& a)
{
	return _Integer64<T>(0)-a;
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
DLL _Integer64<T>& operator+=(_Integer64<T>& a,const _Integer64<T>& b)
{
	a=a+b;
	return a;
}
template<class T>
DLL _Integer64<T>& operator-=(_Integer64<T>& a,const _Integer64<T>& b)
{
	a=a-b;
	return a;
}
static inline _Integer64<uint> __mul32__(uint a,uint b)
{
	uint alow=a&((1<<16)-1),
		ahigh=a>>16,
		blow=b&((1<<16)-1),
		bhigh=b>>16;
	uint abhigh=ahigh*bhigh,
		ablow=alow*blow,
		abmid1=alow*bhigh,
		abmid2=ahigh*blow;
	uint abmid=abmid1+abmid2;
	uint cmid=(abmid<abmid1?1:0);
	uint low=ablow+(abmid<<16);
	uint clow=(low<ablow?1:0);
	uint high=abhigh+(abmid>>16)+(cmid<<16)+clow;
	return _Integer64<uint>(low,&high);
}
template<class T>
DLL _Integer64<T> Mul64(const _Integer64<T>& a,const _Integer64<T>& b,_Integer64<T>* phigh)
{
	return _Integer64<T>();
}
template<>
DLL _Integer64<uint> Mul64<uint>(const _Integer64<uint>& a,const _Integer64<uint>& b,_Integer64<uint>* phigh)
{
	_Integer64<uint> abhigh=__mul32__(a.high,b.high),
		ablow=__mul32__(a.low,b.low),
		abmid1=__mul32__(a.low,b.high),
		abmid2=__mul32__(a.high,b.low);
	_Integer64<uint> abmid=abmid1+abmid2;
	uint cmid=(abmid<abmid1?1:0);
	_Integer64<uint> low=ablow+_Integer64<uint>(0,&abmid.low);
	uint clow=(low<ablow?1:0);
	if(phigh!=NULL)
	{
		uint dummy=0;
		*phigh=abhigh+_Integer64<uint>(abmid.high,&dummy)
			+_Integer64<uint>(0,&cmid)+_Integer64<uint>(clow);
	}
	return low;
}
template<>
DLL _Integer64<int> Mul64<int>(const _Integer64<int>& a,const _Integer64<int>& b,_Integer64<int>* phigh)
{
	_Integer64<int> ta=a,tb=b;
	bool neg=false;
	ta<_Integer64<int>(0)?(ta=-ta,neg=!neg):0;
	tb<_Integer64<int>(0)?(tb=-tb,neg=!neg):0;
	_Integer64<uint> hmul;
	_Integer64<uint> mul=Mul64<uint>(_Integer64<uint>(ta.low,(uint*)&ta.high),
		_Integer64<uint>(tb.low,(uint*)&tb.high),&hmul);
	_Integer64<int> high(hmul.low,(int*)&hmul.high),
		low(mul.low,(int*)&mul.high);
	if(neg)
	{
		low=-low;
		high=-high-_Integer64<int>(1);
	}
	if(phigh!=NULL)
		*phigh=high;
	return low;
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
DLL string FormatI64(const _Integer64<T>& i)
{
	return "";
}
template<>
DLL string FormatI64<uint>(const _Integer64<uint>& i)
{
	if(i.high==0)
	{
		char num[64];
		sprintf(num,"%u",i.low);
		return num;
	}
	else
	{
		const uint base=1000;
		string strout;
		_Integer64<uint> irem=i;
		char buf[20];
		while(irem.high!=0)
		{
			uint rem,t;
			UInteger64 tmp;
			tmp.high=irem.high/base;
			rem=irem.high-tmp.high*base;
			rem=((rem<<16)|(irem.low>>16));
			tmp.low=rem/base;
			rem=rem-tmp.low*base;
			rem=((rem<<16)|(irem.low&((1<<16)-1)));
			t=rem/base;
			tmp.low=((tmp.low<<16)|t);
			rem=rem-t*base;
			sprintf(buf,"%03u",rem);
			strout=buf+strout;
			irem=tmp;
		}
		sprintf(buf,"%u",irem.low);
		strout=buf+strout;
		return strout;
	}
}
template<>
DLL string FormatI64<int>(const _Integer64<int>& i)
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
template<class T>
DLL string FormatI64Hex(const _Integer64<T>& i)
{
	string low,high;
	char buf[9];
	sprintf(buf,i.high!=0?"%08x":"%x",i.low);
	low=buf;
	if(i.high!=0)
	{
		sprintf(buf,"%x",i.high);
		high=buf;
	}
	return high+low;
}
template<class T>
DLL bool I64FromDec(const string& dec,_Integer64<T>& i)
{
	if(dec.empty())
		return false;
	int size=dec.size();
	int ndigit=size;
	bool neg=false;
	if(dec.front()=='-')
	{
		neg=true;
		ndigit--;
	}
	if(ndigit==0)
		return false;
	int nsec=(ndigit-1)/3;
	const _Integer64<uint> base(1000);
	_Integer64<uint> acc(0);
	char buf[4]={0,0,0,0};
	for(int p=nsec;p>=0;p--)
	{
		int sn=p*3;
		int l=(ndigit<sn+3?ndigit-sn:3);
		char* pbuf=buf+3;
		for(int n=0;n<l;n++)
		{
			int pos=size-sn-n-1;
			if(dec[pos]<'0'||dec[pos]>'9')
				return false;
			*(--pbuf)=dec[pos];
		}
		_Integer64<uint> seg(0),high(0),tmp;
		sscanf(pbuf,"%u",&seg.low);
		acc=Mul64(acc,base,&high);
		if(high!=_Integer64<uint>(0))
			return false;
		tmp=acc;
		acc=acc+seg;
		if(acc<tmp)
			return false;
	}
	_Integer64<T> sacc(acc.low,(T*)&acc.high);
	i=(neg?-sacc:sacc);
	return true;
}
template<class T>
DLL bool I64FromHex(const string& hex,_Integer64<T>& i)
{
	int low=0;
	T high=0;
	int size=hex.size();
	if(size>16)
		return false;
	for(int n=0;n<size;n++)
	{
		char c=hex[size-1-n];
		uint d;
		if(c>='A'&&c<='F')
			d=int(c-'A')+10;
		else if(c>='a'&&c<='f')
			d=int(c-'a')+10;
		else if(c>='0'&&c<='9')
			d=int(c-'0');
		else
			return false;
		if(n<8)
			low|=(d<<(n*4));
		else
			high|=(d<<((n-8)*4));
	}
	i=_Integer64<T>(low,&high);
	return true;
}
DLL void __unused_int64_func__()
{
	Integer64 int64_1,int64_2;
	UInteger64 uint64_1,uint64_2;
	Integer64 sum=int64_1+int64_2,
		diff=int64_1-int64_2;
	UInteger64 usum=uint64_1+uint64_2,
		udiff=uint64_1-uint64_2;
	int64_1+=int64_2;
	int64_1-=int64_2;
	uint64_1+=uint64_2;
	uint64_1-=uint64_2;
	string s;
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
	Mul64(int64_1,int64_2,(Integer64*)NULL);
	Mul64(uint64_1,uint64_2,(UInteger64*)NULL);
	FormatI64(int64_1);
	FormatI64(uint64_1);
	FormatI64Hex(int64_1);
	FormatI64Hex(uint64_1);
	I64FromDec(s,int64_1);
	I64FromDec(s,uint64_1);
	I64FromHex(s,int64_1);
	I64FromHex(s,uint64_1);
}
