#ifndef _INTEGER64_H_
#define _INTEGER64_H_
#include "common.h"
template<class T>
struct DLL _Integer64
{
	T high;
	uint low;
	_Integer64(int l=0,const T* h=NULL);
};
template<class T>
DLL _Integer64<T> operator+(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL _Integer64<T> operator-(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator==(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator!=(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator<(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator>(const _Integer64<T>& a,const _Integer64<T>& b);
typedef _Integer64<int> Integer64;
typedef _Integer64<uint> UInteger64;
#endif