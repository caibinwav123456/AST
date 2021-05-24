static inline char lower_case(char c)
{
	if(c>='A'&&c<='Z')
		return c-'A'+'a';
	return c;
}
static inline string to_lower(const string& str)
{
	string lows;
	char s[2]={0,0};
	for(int i=0;i<(int)str.size();i++)
	{
		s[0]=lower_case(str[i]);
		lows+=s;
	}
	return lows;
}
static uint TranslateSize(const string& str)
{
	char* s=(char*)str.c_str();
	char buf[40],unit[8];
	char *p,*pbuf,*punit;
	for(p=s,pbuf=buf;*p!=0&&pbuf-buf<39;p++,pbuf++)
	{
		if(*p>='0'&&*p<='9')
		{
			*pbuf=*p;
		}
		else
			break;
	}
	if(pbuf-buf==39)
		return 0;
	*pbuf=0;
	for(punit=unit;*p!=0&&punit-unit<7;p++,punit++)
	{
		if((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z'))
		{
			*punit=*p;
		}
		else
			break;
	}
	*punit=0;
	if(punit-unit==7)
		return 0;
	if(*p!=0)
	{
		if(*p!=' '&&*p!='\r'&&*p!='\n'&&*p!='\t')
			return 0;
	}
	int n=0;
	sscanf(buf,"%d",&n);
	string sunit(unit);
	uint nunit=1;
	string lunit=to_lower(sunit);
	if(lunit=="k"||lunit=="kb"||lunit=="kib")
		nunit=1024;
	else if(lunit=="m"||lunit=="mb"||lunit=="mib")
		nunit=1024*1024;
	else if(sunit=="")
		nunit=1;
	else
		return 0;
	return ((uint)n)*nunit;
}
	uint nbuf=0;
	const uint max_nbuf=32;
	printf("Please enter a number of buffers for file cache(1~%d):\n",max_nbuf);
	for(;;)
	{
		char buf[128];
		int n=0;
		scanf("%s",buf);
		sscanf(buf,"%d",&n);
		if(n>0&&n<=(int)max_nbuf)
		{
			nbuf=(uint)n;
			break;
		}
		else
		{
			printf("Please enter a number between 1 and %d:",max_nbuf);
		}
	}
	uint buflen=0;
	const uint min_buflen=1024;
	const uint max_buflen=1024*1024;
	printf("Please enter a number for size of each buffer(1K~1M):\n");
	for(;;)
	{
		char bufsize[128]={0};
		scanf("%s",bufsize);
		uint n=TranslateSize(bufsize);
		if(n>=min_buflen&&n<=max_buflen)
		{
			buflen=n;
			break;
		}
		else
		{
			printf("Please enter a number between 1K and 1M:");
		}
	}
