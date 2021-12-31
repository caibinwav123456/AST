#include "match.h"
static const pair<char,E_WILDCARD_SYM> s_wildcard_chars[]
	={pair<char,E_WILDCARD_SYM>('?',WC_ONE_CHAR),pair<char,E_WILDCARD_SYM>('*',WC_ANY_CHAR)};
static int get_item(const string& pattern,uint& pos,E_WILDCARD_SYM& wc,string& str)
{
	str.clear();
	wc=WC_NONE;
	const char* pstr=pattern.c_str()+pos;
	int start=pos,end=pos;
	bool escape=false;
	for(;*pstr!=0;pstr++,end++)
	{
		if(escape)
		{
			escape=false;
			str+=pattern.substr(start,end-start-1)+pattern.substr(end,1);
			start=end+1;
		}
		else
		{
			if(*pstr=='\\')
			{
				escape=true;
				continue;
			}
			char c=pattern[end];
			for(int i=0;i<sizeof(s_wildcard_chars)/sizeof(pair<char,E_WILDCARD_SYM>);i++)
			{
				if(c==s_wildcard_chars[i].first)
				{
					wc=s_wildcard_chars[i].second;
					goto end;
				}
			}
		}
	}
	if(escape)
		return ERR_INVALID_MPATTERN;
end:
	pos=(end==(int)pattern.size()?end:end+1);
	str+=pattern.substr(start,end-start);
	return 0;
}
int CMatch::Compile(const string& pattern,bool* pbwildcard)
{
	int ret=0;
	uint size=pattern.size(),pos=0;
	bool wildcard=false;
	MatchItem item;
	compiled_array.clear();
	while(0==(ret=get_item(pattern,pos,item.wc_sym,item.str_seg)))
	{
		if(item.wc_sym!=WC_NONE)
			wildcard=true;
		compiled_array.push_back(item);
		if(pos>=size)
			break;
	}
	if(ret!=0)
		compiled_array.clear();
	else if(pbwildcard!=NULL)
		*pbwildcard=wildcard;
	return ret;
}
bool CMatch::Match(const string& str_for_match)
{
	uint pos=0;
	E_WILDCARD_SYM prev_sym=WC_NONE;
	MatchItem dummy;
	for(int i=0;i<=(int)compiled_array.size();i++)
	{
		bool last=(i==(int)compiled_array.size());
		const MatchItem& item=(last?dummy:compiled_array[i]);
		uint next_pos=(last?str_for_match.size():str_for_match.find(item.str_seg,pos));
		switch(prev_sym)
		{
		case WC_NONE:
			if(next_pos!=pos)
				return false;
			break;
		case WC_ONE_CHAR:
			if(next_pos!=pos+1)
				return false;
			break;
		case WC_ANY_CHAR:
			if(next_pos==(uint)string::npos)
				return false;
			break;
		default:
			return false;
		}
		if(!last)
		{
			pos=next_pos+item.str_seg.size();
			prev_sym=item.wc_sym;
		}
	}
	return true;
}
