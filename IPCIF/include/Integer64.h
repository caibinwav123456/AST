#ifndef _INTEGER64_H_
#define _INTEGER64_H_
#include "common.h"
#include <string>
using namespace std;
#pragma pack(push,8)
template<class T>
struct DLL _Integer64
{
	T high;
	uint low;
	_Integer64(int l=0,const T* h=NULL);
};
typedef _Integer64<int> Integer64;
typedef _Integer64<uint> UInteger64;
#pragma pack(pop)
template<class T>
DLL _Integer64<T> operator-(const _Integer64<T>& a);
template<class T>
DLL _Integer64<T> operator+(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL _Integer64<T> operator-(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL _Integer64<T>& operator+=(_Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL _Integer64<T>& operator-=(_Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL _Integer64<T> Mul64(const _Integer64<T>& a,const _Integer64<T>& b,_Integer64<T>* phigh=NULL);
template<class T>
DLL bool operator==(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator!=(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator<(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL bool operator>(const _Integer64<T>& a,const _Integer64<T>& b);
template<class T>
DLL string FormatI64(const _Integer64<T>& i);
template<class T>
DLL string FormatI64Hex(const _Integer64<T>& i);
template<class T>
DLL bool I64FromDec(const string& dec,_Integer64<T>& i);
template<class T>
DLL bool I64FromHex(const string& hex,_Integer64<T>& i);
#endif