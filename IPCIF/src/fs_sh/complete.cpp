#include "complete.h"
#ifdef LINUX
#include "sh_cmd_linux.h"
#else
#include "sh_cmd_fs.h"
#endif
#include <stdio.h>
#include <string.h>
#include <algorithm>
#define is_quote(c) ((c)=='\"'||(c)=='\'')
#define MAX_FLIST_CHAR 80
#define MAX_NUM_FLIST 8

static const char spec_file_char[]={' ',';','@','|','<','>','`','^','!','?','\"','\'',
	'(',')','[',']','$','\\','&'};
struct spec_fchar_verifier
{
	bool alphabet[256];
	spec_fchar_verifier()
	{
		memset(alphabet,0,256*sizeof(bool));
		for(int i=0;i<(int)(sizeof(spec_file_char)/sizeof(char));i++)
			alphabet[(int)spec_file_char[i]]=true;
	}
	bool is_spec(char c)
	{
		int i=(int)c;
		if(i<0||i>=128)
			return true;
		else
			return alphabet[i];
	}
};
struct sort_rec
{
	string* file;
};
static bool less_rec(const sort_rec& a,const sort_rec& b)
{
	return *a.file<*b.file;
}
char need_quote(const string& file)
{
	static spec_fchar_verifier vrf;
	bool need=false;
	for(const char* p=file.c_str();*p!=0;p++)
	{
		if(vrf.is_spec(*p))
		{
			if(!need)
				need=true;
			if(is_quote(*p))
			{
				return *p=='\"'?'\'':'\"';
			}
		}
	}
	if(need)
		return '\'';
	return 0;
}
string quote_file(const string& file)
{
	char q=need_quote(file);
	if(q==0)
		return file;
	else
	{
		char s[2]={q,0};
		return s+file+s;
	}
}
static bool is_cmd_cd(const string& cmd)
{
	const char *start=NULL,*end=NULL,*s=cmd.c_str();
	for(const char* p=s;*p!=0;p++)
	{
		if(start==NULL&&*p!=' ')
			start=p;
		if(start!=NULL&&*p==' ')
		{
			end=p;
			break;
		}
	}
	if(start==NULL||end==NULL)
		return false;
	return cmd.substr(start-s,end-start)=="cd";
}
static void find_cplt_start(const char* cmd,uint pos,const char*& start)
{
	if(pos>(uint)strlen(cmd))
	{
		start=NULL;
		return;
	}
	int p=(int)pos;
	int space=0,quote=0;
	bool bs=false,bq=false,inner=false;
	char cq=0;
	for(int i=p-1;i>=0;i--)
	{
		if((!bs)&&cmd[i]==' ')
		{
			bs=true;
			space=i;
		}
		if(is_quote(cmd[i]))
		{
			if(!bq)
			{
				bq=true;
				quote=i;
				cq=cmd[i];
				inner=true;
			}
			else
			{
				if(cmd[i]!=cq)
				{
					start=NULL;
					return;
				}
				inner=!inner;
			}
		}
	}
	if(!bq)
	{
		start=(bs?cmd+space+1:cmd);
	}
	else if(inner)
	{
		start=cmd+quote;
	}
	else
	{
		start=((bs&&space>quote)?cmd+space+1:NULL);
	}
}
static bool filter_files(const string& filter,vector<string>& files,vector<sort_rec>& sort_files,bool dir)
{
	sort_files.clear();
	bool dot=(filter=="."||filter=="..");
	sort_rec rec;
	for(int i=0;i<(int)files.size();i++)
	{
		if(files[i].size()>=filter.size()&&files[i].substr(0,filter.size())==filter)
		{
			bool b=true;
			if(dir)
			{
				if(files[i].empty()||files[i].back()!='/')
					b=false;
			}
			if(b&&!dot&&(files[i]=="./"||files[i]=="../"))
				b=false;
			if(b)
			{
				rec.file=&files[i];
				sort_files.push_back(rec);
			}
		}
	}
	sort(sort_files.begin(),sort_files.end(),less_rec);
	bool empty=((!files.empty())&&filter.empty()&&sort_files.empty());
	return empty;
}
static uint match_files(const vector<sort_rec>& files)
{
	if(files.empty())
		return 0;
	for(uint n=0;;n++)
	{
		if(n>=files[0].file->size())
			return n;
		char c=files[0].file->at(n);
		for(int i=1;i<(int)files.size();i++)
		{
			if(n>=files[i].file->size()||files[i].file->at(n)!=c)
			{
				return n;
			}
		}
	}
	return 0;
}
static inline void display_candt_list(const vector<sort_rec>& files)
{
	uint nc=0,nf=0;
	for(int i=0;i<(int)files.size();i++)
	{
		if(nc>=MAX_FLIST_CHAR||nf>=MAX_NUM_FLIST)
		{
			printf("\n");
			nc=0;
			nf=0;
		}
		else if(i!=0)
			printf("\t");
		nc+=files[i].file->size();
		nf++;
		printf("%s",quote_file(*files[i].file).c_str());
	}
	if(nc>0||nf>0)
		printf("\n");
}
void complete(sh_context* ctx)
{
	string& cmd=ctx->cmd;
	uint& icmd=ctx->icmd;
	const char* start;
	find_cplt_start(cmd.c_str(),icmd,start);
	if(start==NULL)
		return;
	uint offset=start-cmd.c_str();
	char quote=0;
	if(offset<icmd&&is_quote(cmd[offset]))
	{
		quote=cmd[offset];
		offset++;
	}
	string check=cmd.substr(offset,icmd-offset);
	int pos_slash=check.rfind("/");
	string dir,nodir;
	if(pos_slash!=string::npos)
	{
		if(pos_slash==0)
			dir="/";
		else
			dir=check.substr(0,pos_slash);
		nodir=check.substr(pos_slash+1);
	}
	else
	{
		dir="";
		nodir=check;
	}
	vector<string> files;
	vector<sort_rec> sfiles;
#ifdef CONVERT_FULL_PATH
	string fulldir;
	if(0!=get_full_path(ctx->pwd,dir,fulldir))
		return;
	list_cur_dir_files(fulldir,files);
#else
	list_cur_dir_files(dir,files);
#endif
	bool empty_dir=filter_files(nodir,files,sfiles,is_cmd_cd(cmd.substr(0,icmd)));
	if(sfiles.empty()&&!empty_dir)
		return;
	uint nm=match_files(sfiles);
	uint rem=nm-nodir.size();
	if(rem!=0||sfiles.size()==1||empty_dir)
	{
		string add;
		bool bdir=false;
		if(!empty_dir)
		{
			add=sfiles[0].file->substr(nodir.size(),rem);
			bdir=(sfiles[0].file->at(nm-1)=='/');
		}
		if(quote==0&&!empty_dir)
		{
			quote=need_quote(sfiles[0].file->substr(0,nm));
			if(quote!=0)
			{
				char s[2]={quote,0};
				print_back(icmd-offset);
				cmd.insert(offset,s);
				icmd++;
				printf("%s",cmd.substr(offset,icmd-offset).c_str());
			}
		}
		if((!bdir&&sfiles.size()==1)||empty_dir)
		{
			if(quote!=0)
				add+=quote;
			if(!(check.empty()&&empty_dir))
				add+=" ";
		}
		cmd.insert(icmd,add);
		icmd+=add.size();
		printf("%s",add.c_str());
		printf("%s",cmd.substr(icmd).c_str());
		print_back(cmd.size()-icmd);
	}
	else
	{
		printf("\n");
		display_candt_list(sfiles);
		print_prompt(ctx);
		printf("%s",cmd.c_str());
		print_back(cmd.size()-icmd);
	}
}
