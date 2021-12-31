#ifndef _MATCH_H_
#define _MATCH_H_
#include "common.h"
#include <string>
#include <vector>
using namespace std;
enum E_WILDCARD_SYM
{
	WC_NONE=0,
	WC_ONE_CHAR,
	WC_ANY_CHAR,
};
class DLL CMatch
{
public:
	int Compile(const string& pattern,bool* pbwildcard=NULL);
	bool Match(const string& str_for_match);
private:
	struct MatchItem
	{
		string str_seg;
		E_WILDCARD_SYM wc_sym;
	};
	vector<MatchItem> compiled_array;
};
#endif
