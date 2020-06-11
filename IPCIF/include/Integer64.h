#ifndef _INTEGER64_H_
#define _INTEGER64_H_
#include "common.h"
#include <string>
using namespace std;
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
template<class T>
DLL string FormatI64(_Integer64<T> i);
typedef _Integer64<int> Integer64;
typedef _Integer64<uint> UInteger64;
#endif