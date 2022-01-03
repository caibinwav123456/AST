#include "sh_cmd_fs.h"
#include "sysfs.h"
#include "path.h"
#include "match.h"
#include <string.h>
#include <assert.h>
#define t_cat_clean_priv(st_priv) cat_clean_priv(st_priv,__tp_buf__)
#define t_write_clean_priv(st_priv) write_clean_priv(st_priv,pipe_next,__tp_buf__)
#define t_print_byte(bbin,b) print_binary_byte(bbin,b,pipe_next)
#define IS_DISP_CHAR(c) ((c)>=32&&(c)<=126)
//echo cat fmt hex write cp mv rm mkdir setlen touch print
static int echo_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(args.empty()||!args[0].second.empty())
		return_t_msg(ERR_INVALID_CMD,"bad command format\n");
	for(int i=1;i<(int)args.size();i++)
	{
		if(i>1)
			ts_output(" ");
		string out=args[i].first+(args[i].second.empty()?"":"="+args[i].second);
		ts_output(out.c_str());
	}
	if(args.size()>1)
		ts_output("\n");
	return 0;
}
DEF_SH_CMD(echo,echo_handler,
	"output arguments in a formatted way.",
	"The echo command output arguments in its original order, "
	"except that the arguments are segregated by a single space character "
	"no matter how many space characters are between the adjancent arguments "
	"in the original list.\n"
);
struct st_priv_cat
{
	vector<st_path_handle> files;
	Integer64 offset;
	Integer64 len;
	uint seektype;
	st_priv_cat():offset(0),len(-1),seektype(SEEK_BEGIN){}
};
static bool len_vrf(const Integer64& i)
{
	return i>Integer64(0);
}
static inline bool parse_i64(const string& str,Integer64& i,Integer64 def=0,bool (*verifier)(const Integer64&)=NULL)
{
	if(str.empty())
	{
		i=def;
		return true;
	}
	bool ret;
	Integer64 iret;
	if((int)str.size()>2&&str.substr(0,2)=="0x")
		ret=I64FromHex(str.substr(2),iret);
	else
		ret=I64FromDec(str,iret);
	if(verifier!=NULL)
		ret=(ret&&verifier(iret));
	if(ret)
		i=iret;
	return ret;
}
int tclean_hfile(const st_path_handle& file,Pipe* pipe,byte*& buf)
{
	int ret=0;
	void* hfile=file.hfile;
	if(VALID(hfile))
		if(0!=(ret=fs_perm_close(hfile)))
			tprintf(pipe,buf,"\'%s\': file close error: %s.\n",file.path.c_str(),get_error_desc(ret));
	return ret;
}
int tclean_hfiles(const vector<st_path_handle>& vfiles,Pipe* pipe,byte*& buf)
{
	int ret=0;
	for(int i=0;i<(int)vfiles.size();i++)
	{
		int retc;
		if(0!=(retc=fs_perm_close(vfiles[i].hfile)))
		{
			tprintf(pipe,buf,"\'%s\': file close error: %s.\n",vfiles[i].path.c_str(),get_error_desc(retc));
			ret=retc;
		}
	}
	return ret;
}
static inline int cat_clean_priv(st_priv_cat*& stcat,byte*& buf)
{
	int ret=tclean_hfiles(stcat->files,NULL,buf);
	delete stcat;
	stcat=NULL;
	return ret;
}
static int cat_pred_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(is_pre_revoke(param))
		return t_cat_clean_priv(*(st_priv_cat**)&(param->priv));
	if(pipe!=NULL)
		return_msg(ERR_INVALID_CMD,"The command \'%s\' cannot be used with \'|\'\n",cmd.c_str());
	st_priv_cat* newcat=new st_priv_cat;
	st_path_handle ph;
	string fullpath;
	bool secset=false;
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(arg.first[0]=='-')
		{
			if((int)arg.first.size()>1
				&&arg.first[1]=='-')
			{
				int pos;
				if(arg.first.substr(2)=="sec"
					&&(pos=arg.second.find(":"))!=string::npos)
				{
					if(parse_i64(arg.second.substr(0,pos),newcat->offset)
						&&parse_i64(arg.second.substr(pos+1),newcat->len,-1,len_vrf))
					{
						secset=true;
						if((int)newcat->files.size()>1)
						{
							t_cat_clean_priv(newcat);
							return_msg(ERR_INVALID_PARAM,"--sec can not be used with multiple files\n");
						}
						continue;
					}
				}
			}
			else
			{
				bool error=false;
				if(!arg.second.empty())
				{
					t_cat_clean_priv(newcat);
					return_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
				}
				for(int i=1;i<(int)arg.first.size();i++)
				{
					switch(arg.first[i])
					{
					case 'e':
						secset=true;
						if((int)newcat->files.size()>1)
						{
							t_cat_clean_priv(newcat);
							return_msg(ERR_INVALID_PARAM,"-e can not be used with multiple files\n");
						}
						newcat->seektype=SEEK_END;
						break;
					default:
						error=true;
						break;
					}
				}
				if(!error)
					continue;
			}
			t_cat_clean_priv(newcat);
			return_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
		}
		else
		{
			if(secset&&!newcat->files.empty())
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_INVALID_PARAM,"--sec can not be used with multiple files\n");
			}
			if(!arg.second.empty())
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_INVALID_PARAM,"the path \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
			}
			int ret=0;
			ph.path=arg.first;
			if(0!=(ret=get_full_path(ctx->pwd,ph.path,fullpath)))
			{
				t_cat_clean_priv(newcat);
				return_msg(ret,"\'%s\': invalid path\n",ph.path.c_str());
			}
			ph.hfile=fs_open((char*)fullpath.c_str(),FILE_OPEN_EXISTING|FILE_READ);
			if(!VALID(ph.hfile))
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_OPEN_FILE_FAILED,"\'%s\': can not open file for reading\n",ph.path.c_str());
			}
			newcat->files.push_back(ph);
		}
	}
	if(newcat->files.empty())
	{
		t_cat_clean_priv(newcat);
		return_msg(ERR_INVALID_PARAM,"no file specified\n");
	}
	param->priv=newcat;
	return 0;
}
static inline int cat_one_file(cmd_param_st* param,void* hfile,const string& path,Integer64 off,Integer64 len,uint seektype)
{
	common_sh_args(param);
	int ret=0;
	const uint buflen=1024;
	byte buf[buflen];
	Integer64 once(buflen),zero(0);
	bool infinite=(len==Integer64(-1));
	uint rdlen=0;
	if(0!=(ret=fs_seek(hfile,seektype,off.low,&off.high)))
		return_msg(ret,"cat file \'%s\' error: %s\n",path.c_str(),get_error_desc(ret));
	while(len>zero||infinite)
	{
		uint readlen=(len<once&&!infinite?len.low:buflen);
		if(0!=(ret=fs_read(hfile,buf,readlen,&rdlen)))
			return_msg(ret,"cat file \'%s\' error: %s\n",path.c_str(),get_error_desc(ret));
		if(rdlen==0)
			break;
		tb_output(buf,rdlen);
		infinite?0:(len-=Integer64(rdlen));
	}
	return 0;
}
static int cat_handler(cmd_param_st* param)
{
	common_sh_args(param);
	st_priv_cat* catdata=(st_priv_cat*)param->priv;
	assert(catdata!=NULL);
	assert((int)catdata->files.size()==1
		||(catdata->offset==Integer64(0)
		&&catdata->len==Integer64(-1)
		&&catdata->seektype==SEEK_BEGIN));
	int ret=0;
	for(int i=0;i<(int)catdata->files.size();i++)
	{
		if(0!=(ret=cat_one_file(param,catdata->files[i].hfile,catdata->files[i].path,catdata->offset,catdata->len,catdata->seektype)))
			break;
	}
	int retc=t_cat_clean_priv(*(st_priv_cat**)&(param->priv));
	if(ret==0)
		ret=retc;
	return ret;
}
DEF_SH_PRED_CMD(cat,cat_handler,cat_pred_handler,
	"display content of the file(s) specified.",
	"Format:\n\tcat [options] [file1] [file2] ...\n"
	"The cat command outputs binary contents of specified file(s) consecutively.\n"
	"Content range can be specified if there is a single file in the list.\n"
	"Options:\n"
	"--sec=[start]:[length]\n"
	"\tThis option specifies the content range in count of bytes to output, "
	"the start and length values must be both within signed 64-bit binary integers, "
	"and can be either decimal or heximal, with heximal values prefixed by \'0x\'.\n"
	"The start and length values may be ignored, when ignored, start defaults to 0, "
	"and the output length defaults to extend to the end of the file.\n"
	"-e\n"
	"\tUse this option along with --sec, it indicates the start value specified by "
	"--sec is calculated from the end of the file, "
	"this implies only a negative start value will be sensible.\n"
	"Both --sec and -e must be used with a single file contained in the file list.\n"
);
struct st_priv_write
{
	st_path_handle file;
	Integer64 offset;
	bool output_stat;
	st_priv_write():offset(0),output_stat(true){file.hfile=NULL;}
};
static inline int write_clean_priv(st_priv_write*& stwrite,Pipe* pipe,byte*& buf)
{
	int ret=tclean_hfile(stwrite->file,pipe,buf);
	delete stwrite;
	stwrite=NULL;
	return ret;
}
static int write_pred_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(is_pre_revoke(param))
		return t_write_clean_priv(*(st_priv_write**)&(param->priv));
	if(pipe==NULL)
		return_msg(ERR_INVALID_CMD,"The command \'%s\' cannot be used without \'|\'\n",cmd.c_str());
	st_priv_write* newwrite=new st_priv_write;
	string fullpath;
	bool file_arg=false;
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(arg.first[0]=='-')
		{
			if((int)arg.first.size()>1
				&&arg.first[1]=='-')
			{
				if(arg.first.substr(2)=="off")
				{
					if(parse_i64(arg.second,newwrite->offset))
						continue;
				}
			}
			else
			{
				bool error=false;
				if(!arg.second.empty())
				{
					t_write_clean_priv(newwrite);
					return_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
				}
				for(int i=1;i<(int)arg.first.size();i++)
				{
					switch(arg.first[i])
					{
					case 'n':
						newwrite->output_stat=false;
						break;
					default:
						error=true;
						break;
					}
				}
				if(!error)
					continue;
			}
			t_write_clean_priv(newwrite);
			return_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
		}
		else
		{
			if(file_arg)
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_INVALID_PARAM,"can not write to multiple files\n");
			}
			if(!arg.second.empty())
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_INVALID_PARAM,"the path \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
			}
			file_arg=true;
			int ret=0;
			newwrite->file.path=arg.first;
			if(0!=(ret=get_full_path(ctx->pwd,newwrite->file.path,fullpath)))
			{
				t_write_clean_priv(newwrite);
				return_msg(ret,"\'%s\': invalid path\n",arg.first.c_str());
			}
			newwrite->file.hfile=fs_open((char*)fullpath.c_str(),FILE_OPEN_ALWAYS|FILE_READ|FILE_WRITE|FILE_EXCLUSIVE_WRITE);
			if(!VALID(newwrite->file.hfile))
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_OPEN_FILE_FAILED,"\'%s\': can not open file for writing\n",arg.first.c_str());
			}
		}
	}
	if(!file_arg)
	{
		t_write_clean_priv(newwrite);
		return_msg(ERR_INVALID_PARAM,"no file specified\n");
	}
	param->priv=newwrite;
	return 0;
}
static int write_handler(cmd_param_st* param)
{
	common_sh_args(param);
	st_priv_write* writedata=(st_priv_write*)param->priv;
	assert(writedata!=NULL);
	assert(VALID(writedata->file.hfile)
		&&!writedata->file.path.empty());
	int ret=0;
	if(0!=(ret=fs_seek(writedata->file.hfile,SEEK_BEGIN,writedata->offset.low,&writedata->offset.high)))
	{
		t_output("\'%s\': seek for writing file error: %s\n",writedata->file.path.c_str(),get_error_desc(ret));
		goto end;
	}
	{
		const uint buflen=1024;
		byte buf[buflen];
		uint n=0,acc=0,wrlen=0;
		while((n=pipe->Recv(buf,buflen))>0)
		{
			if(0!=(ret=(fs_write(writedata->file.hfile,buf,n,&wrlen)))
				||wrlen!=n)
			{
				ret==0?(ret=ERR_FILE_IO):0;
				t_output("\'%s\': write file error: %s\n",writedata->file.path.c_str(),get_error_desc(ret));
				break;
			}
			acc+=n;
		}
		if(ret==0&&writedata->output_stat)
			t_output("Total %u bytes are written.\n",acc);
	}
end:
	if(ret==0)
		set_used_pipe(param);
	int retc=t_write_clean_priv(*(st_priv_write**)&(param->priv));
	if(ret==0)
		ret=retc;
	return ret;
}
DEF_SH_PRED_CMD(write,write_handler,write_pred_handler,
	"write binary data into a file, for details, see \'help write\'.",
	"Format:\n\t(pre-commands) | write [options] (file-path)\n"
	"NOTE: THIS COMMAND NEEDS TO BE USED WITH \'|\' TO PROVIDE A BINARY STREAM TO BE WRITTEN.\n"
	"The write command writes a stream of binary data into a file, if the file does not exist, "
	"it will be created.\n"
	"The offset to write to the file can be specified as well.\n"
	"Options:\n"
	"--off=[start-offset]\n"
	"\tThis option specifies the offset in count of bytes to start writing. "
	"The start-offset value must be within a signed 64-bit binary integer, "
	"and can be either decimal or heximal, when using a heximal value, it should be prefixed by \'0x\'.\n"
	"The start-offset value may be ignored, when ignored, it defaults to 0.\n"
	"-n\n"
	"\tUse this option to prohibit report after a successfull write of data.\n"
);
static int hex_pred_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(is_pre_revoke(param))
		return 0;
	if(pipe!=NULL)
		return_msg(ERR_INVALID_CMD,"The command \'%s\' cannot be used with \'|\'\n",cmd.c_str());
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(!arg.second.empty())
			return_msg(ERR_INVALID_PARAM,"\'%s=%s\': invalid parameter, quotes needed.\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first.empty())
			continue;
		if(arg.first[0]!='i')
		{
			if(arg.first.size()%2!=0)
				return_msg(ERR_INVALID_PARAM,"\'%s\': invalid parameter\n",arg.first.c_str());
			for(int j=0;j<(int)arg.first.size();j++)
			{
				char c=arg.first[j];
				if(!((c>='0'&&c<='9')
					||(c>='a'&&c<='f')
					||(c>='A'&&c<='F')))
					return_msg(ERR_INVALID_PARAM,"\'%s\': invalid parameter\n",arg.first.c_str());
			}
		}
	}
	return 0;
}
static inline byte gen_bits(char c)
{
	if(c>='0'&&c<='9')
		return c-'0';
	else if(c>='a'&&c<='f')
		return c-'a'+10;
	else if(c>='A'&&c<='F')
		return c-'A'+10;
	else
		return 0;
}
static int hex_handler(cmd_param_st* param)
{
	common_sh_args(param);
	for(int i=1;i<(int)args.size();i++)
	{
		const string& arg=args[i].first;
		if(arg.empty())
			continue;
		if(arg[0]=='i')
		{
			ts_output(arg.c_str()+1);
		}
		else
		{
			uint outlen=arg.size()/2;
			byte* binbuf=new byte[outlen];
			for(int j=0;j<(int)outlen;j++)
			{
				const char* pstr=arg.c_str()+j*2;
				binbuf[j]=((gen_bits(pstr[0])<<4)|gen_bits(pstr[1]));
			}
			tb_output(binbuf,outlen);
			delete[] binbuf;
		}
	}
	return 0;
}
DEF_SH_PRED_CMD(hex,hex_handler,hex_pred_handler,
	"generate sequential binary data.",
	"Format:\n\thex [item1] [item2] [item3] ...\n"
	"The hex command generates a stream of binary data according to the arguments.\n"
	"The stream of data is output to the stdout or can be redirected to files.\n"
	"Each item may be specified in two different ways(styles):\n"
	"1. Heximal bytes\n"
	"\tThis type of data are strings of 8-bit heximal numbers that indicate byte streams, "
	"like 1f, 2e, abcd, fedcba, etc. The heximal bytes can be combined together to form a heximal string.\n"
	"2. ASCII strings\n"
	"\tThis type of data are normal ansi strings preceded with a single ascii character \'i\'. "
	"Quotes(\' or \") are needed when the string contains spaces or other special characters. "
	"The escape character \'\\\' must be used when the quoted string contains special characters other than spaces.\n"
);
struct st_priv_fmt
{
	bool bbin;
	bool bcompact;
	bool bshow_ascii;
	bool bshow_addr;
	uint line_len;
	st_priv_fmt()
	{
		bbin=false;
		bcompact=false;
		bshow_ascii=false;
		bshow_addr=false;
		line_len=0;
	}
};
static inline void fmt_clean_priv(st_priv_fmt*& stfmt)
{
	delete stfmt;
	stfmt=NULL;
}
static int fmt_pred_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(is_pre_revoke(param))
	{
		fmt_clean_priv(*(st_priv_fmt**)&(param->priv));
		return 0;
	}
	if(pipe==NULL)
		return_msg(ERR_INVALID_CMD,"The command \'%s\' cannot be used without \'|\'\n",cmd.c_str());
	st_priv_fmt* newfmt=new st_priv_fmt;
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(arg.first[0]=='-')
		{
			if((int)arg.first.size()>1
				&&arg.first[1]=='-')
			{
				if(arg.first.substr(2)=="linelen")
				{
					uint linelen=0;
					sscanf(arg.second.c_str(),"%u",&linelen);
					if(linelen>=4&&linelen<=32)
					{
						newfmt->line_len=linelen;
						continue;
					}
				}
			}
			else
			{
				bool error=false;
				if(!arg.second.empty())
				{
					fmt_clean_priv(newfmt);
					return_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
				}
				for(int i=1;i<(int)arg.first.size();i++)
				{
					switch(arg.first[i])
					{
					case 'h':
						//default
						break;
					case 'H':
						newfmt->bcompact=true;
						break;
					case 'b':
						newfmt->bbin=true;
						break;
					case 'B':
						newfmt->bbin=true;
						newfmt->bcompact=true;
						break;
					case 'a':
						newfmt->bshow_ascii=true;
						break;
					case 'r':
						newfmt->bshow_addr=true;
						break;
					default:
						error=true;
						break;
					}
				}
				if(!error)
					continue;
			}
		}
		fmt_clean_priv(newfmt);
		return_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
	}
	if((newfmt->bshow_ascii||newfmt->bshow_addr)&&newfmt->line_len==0)
	{
		const char* ill_option=newfmt->bshow_ascii?"a":"r";
		fmt_clean_priv(newfmt);
		return_msg(ERR_INVALID_PARAM,"--linelen must be specified when using the option -%s\n",ill_option);
	}
	param->priv=newfmt;
	return 0;
}
static inline void print_binary_byte(bool bin,byte b,Pipe* pipe)
{
	if(bin)
	{
		byte rem=b;
		for(int i=0;i<8;i++)
		{
			if(rem&0x80)
				tputs(pipe,"1",1);
			else
				tputs(pipe,"0",1);
			rem<<=1;
		}
	}
	else
	{
		char strhex[5];
		sprintf(strhex,"%02x",(uint)b);
		tputs(pipe,strhex,0,true);
	}
}
static int fmt_handler(cmd_param_st* param)
{
	common_sh_args(param);
	st_priv_fmt* fmtdata=(st_priv_fmt*)param->priv;
	assert(fmtdata!=NULL);
	assert(pipe!=NULL);
	byte bbuf;
	bool ending=true;
	byte* tmpbuf=NULL;
	uint cnt=0,n,addr=0;
	char addr_str[16];
	if(fmtdata->bshow_ascii)
		tmpbuf=new byte[fmtdata->line_len];
	while((n=pipe->Recv(&bbuf,1))>0)
	{
		if(fmtdata->bshow_addr&&ending)
		{
			sprintf(addr_str,"%08x: ",addr);
			ts_output(addr_str);
			addr+=fmtdata->line_len;
		}
		ending=false;
		t_print_byte(fmtdata->bbin,bbuf);
		if(!fmtdata->bcompact)
			ts_output(" ");
		if(fmtdata->line_len>0)
		{
			if(fmtdata->bshow_ascii)
				tmpbuf[cnt]=(IS_DISP_CHAR(bbuf)?bbuf:'.');
			cnt++;
			if(cnt>=fmtdata->line_len)
			{
				cnt=0;
				if(fmtdata->bshow_ascii)
				{
					ts_output("\t");
					tb_output(tmpbuf,fmtdata->line_len);
				}
				ts_output("\n");
				ending=true;
			}
		}
	}
	if(!ending)
	{
		if(fmtdata->bshow_ascii)
		{
			uint nbits=(fmtdata->bbin?8:2)+(fmtdata->bcompact?0:1);
			uint nchar=fmtdata->line_len-cnt;
			nbits*=nchar;
			for(int i=0;i<(int)nbits;i++)
			{
				tb_output(" ",1);
			}
			ts_output("\t");
			tb_output(tmpbuf,cnt);
		}
		ts_output("\n");
	}
	if(tmpbuf!=NULL)
		delete[] tmpbuf;
	set_used_pipe(param);
	fmt_clean_priv(*(st_priv_fmt**)&(param->priv));
	return 0;
}
DEF_SH_PRED_CMD(fmt,fmt_handler,fmt_pred_handler,
	"format display of a sequential binary piped stream.",
	"Format:\n\t(pre-commands) | fmt [options]\n"
	"NOTE: THIS COMMAND NEEDS TO BE USED WITH \'|\' TO PROVIDE A BINARY STREAM TO BE DISPLAYED.\n"
	"The fmt command receives a stream of binary data generated by the precedent command. "
	"Multiple options can be specified to customize the output format.\n"
	"Options:\n"
	"--linelen=(decimal-number)\n"
	"\tThis option specifies the number of bytes shown in each line within the output content. "
	"The bytes count is given in a decimal number between 4 and 32.\n"
	"If this option is not set, the output stream is shown in continuous bytes without line segregation.\n"
	"-h\n"
	"\tThis option specifies that the output stream is displayed in heximal format "
	"with each byte separated by a space character.\n"
	"\tThis is the default format.\n"
	"-H\n"
	"\tThis option specifies that the output stream is displayed in compact heximal format "
	"with no space separations between adjancent bytes.\n"
	"-b\n"
	"\tThis option specifies that the output stream is displayed in binary format "
	"with each byte separated by a space character.\n"
	"-B\n"
	"\tThis option specifies that the output stream is displayed in compact binary format "
	"with no space separations between adjancent bytes.\n"
	"-a\n"
	"\tThe ascii characters is shown besides the output stream.\n"
	"\tThis option must be used together with option \'--linelen\'.\n"
	"-r\n"
	"\tThe offset of each line in the output stream is displayed before the line in 32-bit heximal format.\n"
	"\tThis option must be used together with option \'--linelen\'.\n"
);
struct st_inner_path
{
	union
	{
		bool detect_outer;
		bool outer;
	};
	union
	{
		bool detect_root;
		bool none_identical_root;
	};
	const char* inner;
	string get_inner_path()
	{
		const char* pstr=strrchr(inner,'/');
		return pstr==NULL?"":pstr;
	}
	static string get_root(const char* path)
	{
		const char* pstr=strchr(path,'/');
		return pstr==NULL?"":string(path).substr(0,pstr-path);
	}
};
#define SILENT_RET(ret,bsilent,R,...) \
	{if(!(bsilent)) \
	{ \
		R(ret,##__VA_ARGS__); \
	} \
	else \
	{ \
		return ret; \
	}}
static int check_path(cmd_param_st* param,const string& path,string& fullpath,bool no_output_msg=false,dword* flags=NULL,st_inner_path* inner=NULL)
{
	common_sh_args(param);
	int ret=0;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
		SILENT_RET(ret,no_output_msg,return_t_msg,"\'%s\': invalid path\n",path.c_str());
	if(fullpath.empty())
		SILENT_RET(ERR_INVALID_PATH,no_output_msg,return_t_msg,"\'%s\': invalid path\n",path.c_str());
	if(fullpath[0]=='/')
	{
		vector<string> devs;
		uint def=0;
		if(0!=(ret=fs_list_dev(devs,&def)))
			SILENT_RET(ret,no_output_msg,return_t_msg,"%s\n",get_error_desc(ret));
		fullpath=devs[def]+":"+(fullpath=="/"?"":fullpath);
	}
	string strret;
	dword dummy=0;
	dword* pflag=(flags!=NULL?flags:(inner!=NULL?&dummy:NULL));
	bool detect_outer=false;
	if(inner!=NULL)
	{
		if(inner->detect_outer)
		{
			detect_outer=true;
			inner->outer=false;
		}
		if(inner->detect_root)
		{
			string root1=st_inner_path::get_root(inner->inner),
				root2=st_inner_path::get_root(fullpath.c_str());
			if(root1.empty()||root2.empty())
			{
				ret=ERR_NOT_AVAIL_ON_ROOT_DIR;
				SILENT_RET(ret,no_output_msg,return_t_msg,"%s\n",get_error_desc(ret));
			}
			inner->none_identical_root=(root1!=root2);
		}
	}
	if(0!=(ret=validate_path(fullpath,pflag,NULL,NULL,&strret)))
	{
		if(ret==ERR_PATH_NOT_EXIST&&inner!=NULL&&detect_outer)
		{
			int pos=fullpath.rfind('/');
			if(pos!=string::npos)
			{
				string fullpath_outer=fullpath.substr(0,pos);
				if(0==(ret=validate_path(fullpath_outer,pflag,NULL,NULL,&strret)))
				{
					inner->outer=true;
					return 0;
				}
			}
		}
		SILENT_RET(ret,no_output_msg,return_t_msg,"\'%s\':\n%s",path.c_str(),strret.c_str());
	}
	if(inner!=NULL&&detect_outer&&(FS_IS_DIR(*pflag)||inner->inner==fullpath))
	{
		string inner_path=inner->get_inner_path();
		if(inner_path.empty())
			SILENT_RET(ERR_NOT_AVAIL_ON_ROOT_DIR,no_output_msg,return_t_msg,"\'%s\': cannot move/copy/remove root directory\n",inner->inner);
		inner->outer=true;
		if(fullpath!=inner->inner)
			fullpath+=inner_path;
	}
	return 0;
}
#define CMD_ID_MV 0
#define CMD_ID_CP 1
#define CMD_ID_RM 2
struct file_recurse_ret
{
	int ret;
	string path;
	file_recurse_ret(int _ret=0,const char* str=NULL):ret(_ret),path(str==NULL?"":str){}
};
struct file_recurse_option
{
	bool recur;
	bool verbose;
	bool force;
	bool stop_at_error;
	bool handle_dir;
	bool handle_file;
	file_recurse_option()
	{
		recur=false;
		verbose=false;
		force=false;
		stop_at_error=false;
		handle_dir=true;
		handle_file=true;
	}
};
struct file_recurse_st
{
	cmd_param_st* param;
	CMatch* pmatch;
	file_recurse_option option;
	bool none_identical_root;
	string src;
	string dest;
	string fsrc;
	string fdest;
	vector<file_recurse_ret> ret;
	uint cmd_id;
	file_recurse_st(cmd_param_st* _param,uint _cmd_id):param(_param),pmatch(NULL),cmd_id(_cmd_id),none_identical_root(false){}
};
static int cb_fs_recurse_data(int recret,char* path,dword flags,void* rparam,char dsym)
{
	file_recurse_st* rec=(file_recurse_st*)rparam;
	common_sh_args(rec->param);
	string pathdir;
	if(flags==FILE_TYPE_DIR)
		pathdir=string(path)+"/";
	const char* dpath=(flags==FILE_TYPE_DIR?pathdir.c_str():path);
	if(rec->option.verbose)
	{
		switch(rec->cmd_id)
		{
		case CMD_ID_CP:
			assert(strlen(path)>=rec->fdest.size());
			ts_output((rec->fsrc+string(path).substr(rec->fdest.size())+(flags==FILE_TYPE_DIR?"/":"")).c_str());
			ts_output(" -> ");
			ts_output(dpath);
			ts_output(recret==0?" ok\n":" failed\n");
			break;
		case CMD_ID_RM:
			ts_output("deleting ");
			ts_output(dpath);
			ts_output(recret==0?" ok\n":" failed\n");
			break;
		}
	}
	if(recret!=0)
		rec->ret.push_back(file_recurse_ret(recret,dpath));
	return rec->option.stop_at_error?recret:0;
}
static void output_fsrecur_errors(file_recurse_st* frecur,const char* msg)
{
	common_sh_args(frecur->param);
	if(frecur->ret.empty())
		return;
	ts_output("Trace errors:\n\n");
	for(int i=0;i<(int)frecur->ret.size();i++)
	{
		ts_output("\'");
		ts_output(frecur->ret[i].path.c_str());
		ts_output("\'");
		ts_output(": ");
		ts_output(get_error_desc(frecur->ret[i].ret));
		ts_output("\n");
	}
	ts_output(msg);
}
static int do_move(file_recurse_st& frecur)
{
	common_sh_args(frecur.param);
	int ret=0;
	if(frecur.fdest==frecur.fsrc)
		return 0;
	if(is_subpath(frecur.fdest,frecur.fsrc))
		SILENT_RET(ERR_NOT_AVAIL_ON_SUB_DIR,frecur.option.force,return_t_msg,"\'%s\': cannot move a directory to its sub-directory\n",frecur.fsrc.c_str());
	if(!frecur.option.recur)
	{
		if(0!=(ret=fs_move((char*)(frecur.fsrc.c_str()),(char*)(frecur.fdest.c_str()))))
			SILENT_RET(ret,frecur.option.force,return_t_msg,"\'%s\': %s\n",cmd.c_str(),get_error_desc(ret));
	}
	else
	{
		file_recurse_cbdata cbdata={cb_fs_recurse_data,&frecur};
		if(frecur.option.verbose)
		{
			ts_output("\nmoving \'");
			ts_output(frecur.fsrc.c_str());
			ts_output("\' to non-identical device:\n\n");
			ts_output("firstly, copy files to destination.\n");
		}
		frecur.cmd_id=CMD_ID_CP;
		if(0!=(ret=fs_recurse_copy((char*)(frecur.fsrc.c_str()),(char*)(frecur.fdest.c_str()),&cbdata)))
		{
			if(!frecur.option.force)
				output_fsrecur_errors(&frecur,"\nError occured while copying files!\n\n");
			frecur.cmd_id=CMD_ID_MV;
			return ret;
		}
		if(frecur.option.verbose)
			ts_output("\nsecondly, delete source files.\n");
		frecur.cmd_id=CMD_ID_RM;
		if(0!=(ret=fs_recurse_delete((char*)(frecur.fsrc.c_str()),&cbdata)))
		{
			if(!frecur.option.force)
				output_fsrecur_errors(&frecur,"\nError occured while deleting files!\n\n");
			frecur.cmd_id=CMD_ID_MV;
			return ret;
		}
		frecur.cmd_id=CMD_ID_MV;
	}
	return 0;
}
static int do_copy(file_recurse_st& frecur)
{
	common_sh_args(frecur.param);
	int ret=0;
	if(frecur.fdest==frecur.fsrc)
		return 0;
	if(is_subpath(frecur.fdest,frecur.fsrc))
		SILENT_RET(ERR_NOT_AVAIL_ON_SUB_DIR,frecur.option.force,return_t_msg,"cannot copy a directory to its sub-directory\n");
	file_recurse_cbdata cbdata={cb_fs_recurse_data,&frecur};
	if(!frecur.option.recur)
	{
		if(frecur.option.verbose)
		{
			ts_output(frecur.fsrc.c_str());
			ts_output(" -> ");
			ts_output(frecur.fdest.c_str());
		}
		if(0!=(ret=fs_copy((char*)frecur.fsrc.c_str(),(char*)frecur.fdest.c_str())))
		{
			if(frecur.option.verbose)
				ts_output(" failed\n");
			SILENT_RET(ret,frecur.option.force,return_t_msg,"Error: %s\n",get_error_desc(ret));
		}
		if(frecur.option.verbose)
			ts_output(" ok\n");
	}
	else
	{
		ret=fs_recurse_copy((char*)frecur.fsrc.c_str(),(char*)frecur.fdest.c_str(),&cbdata);
		if(!frecur.option.force)
			output_fsrecur_errors(&frecur,"\nError occured while copying files!\n\n");
	}
	return ret;
}
static int do_delete(file_recurse_st& frecur)
{
	common_sh_args(frecur.param);
	int ret=0;
	file_recurse_cbdata cbdata={cb_fs_recurse_data,&frecur};
	if(!frecur.option.recur)
	{
		if(frecur.option.verbose)
		{
			ts_output("deleting ");
			ts_output(frecur.fsrc.c_str());
		}
		if(0!=(ret=fs_delete((char*)frecur.fsrc.c_str())))
		{
			if(frecur.option.verbose)
				ts_output(" failed\n");
			SILENT_RET(ret,frecur.option.force,return_t_msg,"Error: %s\n",get_error_desc(ret));
		}
		if(frecur.option.verbose)
			ts_output(" ok\n");
	}
	else
	{
		ret=fs_recurse_delete((char*)frecur.fsrc.c_str(),&cbdata);
		if(!frecur.option.force)
			output_fsrecur_errors(&frecur,"\nError occured while deleting files!\n\n");
	}
	return ret;
}
static int cb_fs_recurse_func(char* path,dword flags,void* rparam,char dsym)
{
	file_recurse_st* rec=(file_recurse_st*)rparam;
	rec->fsrc=rec->src+"/"+path;
	rec->ret.clear();
	if((rec->pmatch!=NULL&&!rec->pmatch->Match(path))
		||((!rec->option.handle_dir)&&flags==FILE_TYPE_DIR)
		||((!rec->option.handle_file)&&flags==FILE_TYPE_NORMAL))
		return 0;
	switch(rec->cmd_id)
	{
	case CMD_ID_MV:
		rec->fdest=rec->dest+"/"+path;
		rec->option.recur=(rec->none_identical_root&&flags==FILE_TYPE_DIR);
		do_move(*rec);
		break;
	case CMD_ID_CP:
		rec->fdest=rec->dest+"/"+path;
		do_copy(*rec);
		break;
	case CMD_ID_RM:
		do_delete(*rec);
		break;
	}
	return 0;
}
static inline int init_match(CMatch& match,string& path,bool& wildcard)
{
	int ret=0;
	int pos=path.rfind("/");
	if(0!=(ret=match.Compile(pos==string::npos?path:path.substr(pos+1),&wildcard)))
		return ret;
	if(wildcard)
		path=(pos==string::npos?"":path.substr(0,pos));
	return 0;
}
static int mv_handler(cmd_param_st* param)
{
	common_sh_args(param);
	int pcnt=0;
	bool error=false;
	file_recurse_st frecur(param,CMD_ID_MV);
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(!arg.second.empty())
			return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first[0]=='-')
		{
			for(int i=1;i<(int)arg.first.size();i++)
			{
				switch(arg.first[i])
				{
				case 'v':
					frecur.option.verbose=true;
					frecur.option.force=false;
					break;
				case 'f':
					if(!frecur.option.verbose)
						frecur.option.force=true;
					break;
				case 's':
					frecur.option.stop_at_error=true;
					break;
				case 'd':
					frecur.option.handle_file=false;
					break;
				case 'n':
					frecur.option.handle_dir=false;
					break;
				default:
					return_t_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
				}
			}
			continue;
		}
		pcnt++;
		switch(pcnt)
		{
		case 1:
			frecur.src=arg.first;
			break;
		case 2:
			frecur.dest=arg.first;
			break;
		default:
			error=true;
			break;
		}
		if(error)
			break;
	}
	pcnt!=2?error=true:0;
	if(error)
		return_t_msg(ERR_INVALID_PARAM,"path count not equal to 2\n");
	if(!(frecur.option.handle_dir||frecur.option.handle_file))
		return_t_msg(ERR_INVALID_PARAM,"options \'-d\' and \'-n\' can not be used together\n");
	int ret=0;
	string w_src;
	bool wildcard=false;
	CMatch match;
	if(0!=(ret=init_match(match,frecur.src,wildcard)))
		return_t_msg(ret,"\'%s\': %s\n",frecur.src.c_str(),get_error_desc(ret));
	if(wildcard)
		frecur.pmatch=&match;
	dword flags=0;
	if(0!=(ret=check_path(frecur.param,frecur.src,frecur.fsrc,false,&flags)))
		return ret;
	if(wildcard)
		w_src=frecur.src=frecur.fsrc;
	if(wildcard&&!FS_IS_DIR(flags))
		return_t_msg(ERR_NOT_AVAIL_ON_FILE,"\'%s\': can not use wildcard on none-directories\n",frecur.fsrc.c_str());
	st_inner_path inner={!wildcard,true,frecur.fsrc.c_str()};
	if(0!=(ret=check_path(frecur.param,frecur.dest,frecur.fdest,false,NULL,&inner)))
		return ret;
	if(inner.none_identical_root&&(wildcard||FS_IS_DIR(flags)))
		frecur.none_identical_root=true;
	if(wildcard)
		frecur.dest=frecur.fdest;
	else
	{
		if(!inner.outer)
			return_t_msg(ERR_PATH_ALREADY_EXIST,"the destination path \'%s\' already exists\n",frecur.fdest.c_str());
		frecur.option.recur=frecur.none_identical_root;
	}
	return wildcard?fs_traverse((char*)w_src.c_str(),cb_fs_recurse_func,&frecur):do_move(frecur);
}
DEF_SH_CMD(mv,mv_handler,
	"move a file or a directory to a new path.",
	"Format:\n\tmv [options] (source-path) (destination-path)\n"
	"The mv command moves the file/directory of source-path to destination-path.\n"
	"Wildcard symbol \'*\' can be used as placeholder for 0~any character(s), "
	"\'?\' can be used as placeholder for exactly 1 character to form a match pattern "
	"in the last section of source path and move all the matched files/directories to the destination path.\n"
	"The source path must exist.\n"
	"If the destination path does not exist, the source path will be moved to that path.\n"
	"If the destination path exists and is a directory, it will be moved with the same name as a sub-node of that path, "
	"if it is a normal file, this command will fail.\n"
	"A directory can not be moved to its sub-path because it causes infinite recursion.\n"
	"The root directory of any device can not be used as source or destination path.\n"
	"A mv command that moves a path to its identical path will successfully return and cause no effects.\n"
	"Options:\n"
	"-v\n"
	"\tVerbose. Display detailed information of move process and errors, specially for batch move operation across different devices, overrides -f.\n"
	"-f\n"
	"\tForce. Ignore and do not display error information.\n"
	"-s\n"
	"\tStop at first error. With this option, each process of batch move operations will stop immediately when an error is encountered.\n"
	"-d\n"
	"\tOnly process directories, for path with wildcard symbols.\n"
	"-n\n"
	"\tOnly process normal files, for path with wildcard symbols.\n"
);
static int cp_handler(cmd_param_st* param)
{
	common_sh_args(param);
	int pcnt=0;
	bool error=false;
	file_recurse_st frecur(param,CMD_ID_CP);
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(!arg.second.empty())
			return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first[0]=='-')
		{
			for(int i=1;i<(int)arg.first.size();i++)
			{
				switch(arg.first[i])
				{
				case 'r':
				case 'R':
					frecur.option.recur=true;
					break;
				case 'v':
					frecur.option.verbose=true;
					frecur.option.force=false;
					break;
				case 'f':
					if(!frecur.option.verbose)
						frecur.option.force=true;
					break;
				case 's':
					frecur.option.stop_at_error=true;
					break;
				case 'd':
					frecur.option.handle_file=false;
					break;
				case 'n':
					frecur.option.handle_dir=false;
					break;
				default:
					return_t_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
				}
			}
			continue;
		}
		pcnt++;
		switch(pcnt)
		{
		case 1:
			frecur.src=arg.first;
			break;
		case 2:
			frecur.dest=arg.first;
			break;
		default:
			error=true;
			break;
		}
		if(error)
			break;
	}
	pcnt!=2?error=true:0;
	if(error)
		return_t_msg(ERR_INVALID_PARAM,"path count not equal to 2\n");
	if(!(frecur.option.handle_dir||frecur.option.handle_file))
		return_t_msg(ERR_INVALID_PARAM,"options \'-d\' and \'-n\' can not be used together\n");
	int ret=0;
	string w_src;
	bool wildcard=false;
	CMatch match;
	if(0!=(ret=init_match(match,frecur.src,wildcard)))
		return_t_msg(ret,"\'%s\': %s\n",frecur.src.c_str(),get_error_desc(ret));
	if(wildcard)
		frecur.pmatch=&match;
	dword flags=0;
	if(0!=(ret=check_path(frecur.param,frecur.src,frecur.fsrc,frecur.option.force,&flags)))
		return ret;
	if(wildcard)
		w_src=frecur.src=frecur.fsrc;
	if(wildcard&&!FS_IS_DIR(flags))
		return_t_msg(ERR_NOT_AVAIL_ON_FILE,"\'%s\': can not use wildcard on none-directories\n",frecur.fsrc.c_str());
	st_inner_path inner={!wildcard,false,frecur.fsrc.c_str()};
	if(0!=(ret=check_path(frecur.param,frecur.dest,frecur.fdest,frecur.option.force,NULL,&inner)))
		return ret;
	if(wildcard)
		frecur.dest=frecur.fdest;
	return wildcard?fs_traverse((char*)w_src.c_str(),cb_fs_recurse_func,&frecur):do_copy(frecur);
}
DEF_SH_CMD(cp,cp_handler,
	"copy a file(or files) or a directory(or directories) to a new path.",
	"Format:\n\tcp [options] (source-path) (destination-path)\n"
	"The cp command copies the file/directory of source-path to destination-path, multiple options are supported.\n"
	"Wildcard symbol \'*\' can be used as placeholder for 0~any character(s), "
	"\'?\' can be used as placeholder for exactly 1 character to form a match pattern "
	"in the last section of source path and copy all the matched files/directories to the destination path.\n"
	"The source path must exist.\n"
	"If the destination path does not exist, the source path will be copied to that path.\n"
	"If the destination path exists and is a directory, it will be copied and merged with the same name as a sub-node of that path, "
	"if it is a normal file, this command will fail.\n"
	"A directory can not be copied to its sub-path because it causes infinite recursion.\n"
	"The root directory of any device can not be used as source path.\n"
	"A cp command that copies a path to its identical path will successfully return and cause no effects.\n"
	"Options:\n"
	"-r, -R\n"
	"\tRecursive copy of sub-dir. If the source path is a directory, this indicates the path and all its sub-paths will be recursively copied. "
	"Without this option, only the directory itself will be copied. This option has no effects on normal files.\n"
	"-v\n"
	"\tVerbose. Display detailed information of copy process and errors, overrides -f.\n"
	"-f\n"
	"\tForce. Ignore and do not display error information.\n"
	"-s\n"
	"\tStop at first error. With this option, copy process will stop immediately when an error is encountered.\n"
	"-d\n"
	"\tOnly process directories, for path with wildcard symbols.\n"
	"-n\n"
	"\tOnly process normal files, for path with wildcard symbols.\n"
);
static int rm_handler(cmd_param_st* param)
{
	common_sh_args(param);
	vector<string> vfiles;
	file_recurse_st frecur(param,CMD_ID_RM);
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(!arg.second.empty())
			return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first[0]=='-')
		{
			for(int i=1;i<(int)arg.first.size();i++)
			{
				switch(arg.first[i])
				{
				case 'r':
				case 'R':
					frecur.option.recur=true;
					break;
				case 'v':
					frecur.option.verbose=true;
					frecur.option.force=false;
					break;
				case 'f':
					if(!frecur.option.verbose)
						frecur.option.force=true;
					break;
				case 's':
					frecur.option.stop_at_error=true;
					break;
				case 'd':
					frecur.option.handle_file=false;
					break;
				case 'n':
					frecur.option.handle_dir=false;
					break;
				default:
					return_t_msg(ERR_INVALID_PARAM,"the option \'%s\' is invalid\n",arg.first.c_str());
				}
			}
		}
		else
		{
			vfiles.push_back(arg.first);
		}
	}
	if(!(frecur.option.handle_dir||frecur.option.handle_file))
		return_t_msg(ERR_INVALID_PARAM,"options \'-d\' and \'-n\' can not be used together\n");
	int ret=0;
	for(int i=0;i<(int)vfiles.size();i++)
	{
		int retc=0;
		string w_path;
		bool wildcard=false;
		string& curfile=vfiles[i];
		CMatch match;
		if(0!=(ret=init_match(match,curfile,wildcard)))
			return_t_msg(ret,"\'%s\': %s\n",curfile.c_str(),get_error_desc(ret));
		if(wildcard)
			frecur.pmatch=&match;
		if(0!=(retc=check_path(frecur.param,curfile,frecur.fsrc,frecur.option.force)))
		{
			ret=retc;
			continue;
		}
		if(wildcard)
			w_path=frecur.src=frecur.fsrc;
		if(0!=(retc=(wildcard?fs_traverse((char*)w_path.c_str(),cb_fs_recurse_func,&frecur):do_delete(frecur))))
		{
			ret=retc;
		}
	}
	return ret;
}
DEF_SH_CMD(rm,rm_handler,
	"remove a file(or files) or a directory(or directories).",
	"Format:\n\trm [options] (path1) (path2) ...\n"
	"The rm command deletes the file/directory of paths listed in the path list, multiple options are supported.\n"
	"Wildcard symbol \'*\' can be used as placeholder for 0~any character(s), "
	"\'?\' can be used as placeholder for exactly 1 character to form a match pattern "
	"in the last section of path and delete all the matched files/directories.\n"
	"If a path does not exist, the command will return with no effects.\n"
	"The root directory of any device can not be deleted.\n"
	"Options:\n"
	"-r, -R\n"
	"\tRecursive deletion of sub-dir. If the path is a directory, this indicates the path and all its sub-paths will be recursively deleted. "
	"Without this option, the command will fail if the directory is not empty. This option has no effects on normal files.\n"
	"-v\n"
	"\tVerbose. Display detailed information of deletion process and errors, overrides -f.\n"
	"-f\n"
	"\tForce. Ignore and do not display error information.\n"
	"-s\n"
	"\tStop at first error. With this option, deleting process will stop immediately when an error is encountered.\n"
	"-d\n"
	"\tOnly process directories, for path with wildcard symbols.\n"
	"-n\n"
	"\tOnly process normal files, for path with wildcard symbols.\n"
);
static int mkdir_handler(cmd_param_st* param)
{
	common_sh_args(param);
	vector<string> vpath;
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(arg.first.empty())
			continue;
		if(!arg.second.empty())
			return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first[0]=='-')
		{
			return_t_msg(ERR_INVALID_PATH,"\'%s\': invalid path\n",arg.first.c_str());
		}
		else
			vpath.push_back(arg.first);
	}
	int ret=0;
	string fullpath;
	for(int i=0;i<(int)vpath.size();i++)
	{
		int retc=0;
		string& path=vpath[i];
		if(0!=(retc=get_full_path(ctx->pwd,path,fullpath)))
		{
			t_output("\'%s\': invalid path\n",path.c_str());
			ret=retc;
		}
		if(0!=(retc=fs_mkdir((char*)fullpath.c_str())))
		{
			t_output("\'%s\': create directory failed: %s\n",fullpath.c_str(),get_error_desc(retc));
			ret=retc;
		}
	}
	return ret;
}
DEF_SH_CMD(mkdir,mkdir_handler,
	"create a new directory(or directories).",
	"Format:\n\tmkdir (path1) (path2) ...\n"
	"The mkdir command creates directories of the specified paths. "
	"Each path is created respectively.\n"
	"When a directory is created, its parent directories along its path will be created simultaneously.\n"
	"If the specified path already exists and is a directory, the command will return success with no effects, "
	"if it is a normal file, the command will fail.\n"
);
static int setlen_handler(cmd_param_st* param)
{
	common_sh_args(param);
	string path,fullpath;
	Integer64 length;
	bool lenset=false;
	if(args.size()!=3)
		return_t_msg(ERR_INVALID_PARAM,"the number of command parameters must be 2\n");
	if(!args[1].second.empty())
		return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",args[1].first.c_str(),args[1].second.c_str());
	if(!args[2].second.empty())
		return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",args[2].first.c_str(),args[2].second.c_str());
	path=args[1].first;
	if(!parse_i64(args[2].first,length,0,len_vrf))
		return_t_msg(ERR_INVALID_PARAM,"\'%s\': length not valid\n",args[2].first.c_str());
	int ret=0;
	dword flags=0;
	if(0!=(ret=check_path(param,path,fullpath,false,&flags)))
		return ret;
	if(FS_IS_DIR(flags))
		return_t_msg(ERR_NOT_AVAIL_ON_DIR,"\'%s\' is a directory\n",fullpath.c_str());
	void* hfile=fs_open((char*)fullpath.c_str(),FILE_OPEN_EXISTING|FILE_WRITE);
	if(!VALID(hfile))
		return_t_msg(ERR_OPEN_FILE_FAILED,"\'%s\': open file failed\n",fullpath.c_str());
	if(0!=(ret=fs_set_file_size(hfile,&length.low,(uint*)&length.high)))
	{
		t_output("\'%s\': setlen failed: %s\n",fullpath.c_str(),get_error_desc(ret));
		goto end;
	}
end:
	fs_perm_close(hfile);
	return ret;
}
DEF_SH_CMD(setlen,setlen_handler,
	"set file length.",
	"Format:\n\tsetlen (file-path) (length)\n"
	"The setlen command sets the length of the specified file.\n"
	"The file-path must be an existing path and must indicate a normal file.\n"
	"The length value must be within signed 64-bit binary integers, "
	"and can be either decimal or heximal, with heximal values prefixed by \'0x\'.\n"
	"If the desired length is less than the original file length, "
	"it will be truncated to the desired length.\n"
	"If the desired length is more than the original file length, "
	"the file will be extended to the desired length with the surplus filled with arbitrary bytes.\n"
);
static int touch_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(args.size()<2)
		return_t_msg(ERR_INVALID_PARAM,"there must be at least one path to be touched\n");
	int ret=0;
	string fullpath;
	string strret;
	DateTime datetime[3];
	memset(datetime,0,sizeof(datetime));
	sys_get_date_time(&datetime[fs_attr_modify_date]);
	datetime[fs_attr_access_date]=datetime[fs_attr_modify_date];
	for(int i=1;i<(int)args.size();i++)
	{
		if(!args[i].second.empty())
			return_t_msg(ERR_INVALID_PARAM,"the option \'%s=%s\' is invalid\n",args[i].first.c_str(),args[i].second.c_str());
		int retc=0;
		bool isdir=false;
		const string& path=args[i].first;
		if(0!=(retc=get_full_path(ctx->pwd,path,fullpath)))
		{
			ret=retc;
			t_output("\'%s\': invalid path\n",path.c_str());
			continue;
		}
		dword flags=0;
		if(0!=(retc=validate_path(fullpath,&flags,NULL,NULL,&strret)))
		{
			if(retc!=ERR_PATH_NOT_EXIST)
			{
				ret=retc;
				t_output("\'%s\':\n%s",fullpath.c_str(),strret.c_str());
				continue;
			}
			void* hfile=fs_open((char*)fullpath.c_str(),FILE_CREATE_ALWAYS);
			if(!VALID(hfile))
			{
				ret=ERR_CREATE_FILE_FAILED;
				t_output("\'%s\': create file failed\n",fullpath.c_str());
				continue;
			}
			fs_perm_close(hfile);
			continue;
		}
		else if(FS_IS_DIR(flags))
		{
			ret=ERR_NOT_AVAIL_ON_DIR;
			t_output("\'%s\': is a directory\n",fullpath.c_str());
			isdir=true;
		}
		if(0!=(retc=fs_set_attr((char*)fullpath.c_str(),FS_ATTR_MODIFY_DATE|FS_ATTR_ACCESS_DATE,0,datetime)))
		{
			if(!isdir)
				t_output("\'%s\': update timestamp failed: %s\n",fullpath.c_str(),get_error_desc(retc));
			ret=retc;
		}
	}
	return ret;
}
DEF_SH_CMD(touch,touch_handler,
	"update timestamp of a file/files or create a new empty file if it does not exist.",
	"Format:\n\ttouch (path1) (path2) ...\n"
	"The touch command updates timestamp of the file in the path list to the current time "
	"or creates a new empty file if it does not exist.\n"
	"Each path is processed respectively.\n"
);
static int print_handler(cmd_param_st* param)
{
	common_sh_args(param);
	return 0;
}
DEF_SH_CMD(print,print_handler,
	"show specified/all environment variable(s).",
	""
);
