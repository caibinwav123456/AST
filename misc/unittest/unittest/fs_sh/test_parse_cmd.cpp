#include <stdio.h>
#include <string.h>
#include "parse_cmd.h"

int main()
{
	const char* str="cai bin c=\"a b\" \'b c\'=w \"c a\"=\"f s\"";
	vector<pair<string,string>> option;
	int ret=parse_cmd((const byte*)str,strlen(str),option);
	printf("%d\n",ret);
	for(int i=0;i<(int)option.size();i++)
	{
		printf("%s = %s\n",option[i].first.c_str(),option[i].second.c_str());
	}
	return 0;
}
