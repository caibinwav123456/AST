#include "sh_cmd_fs.h"
#include "sysfs.h"
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
			tprintf(pipe,buf,"file \"%s\" close error: %s.\n",file.path.c_str(),get_error_desc(ret));
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
			tprintf(pipe,buf,"file \"%s\" close error: %s.\n",vfiles[i].path.c_str(),get_error_desc(retc));
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
		return_msg(ERR_INVALID_CMD,"The command \"%s\" cannot be used with \"|\"\n",cmd.c_str());
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
							return_msg(ERR_INVALID_CMD,"--sec can not be used with multiple files\n");
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
					return_msg(ERR_INVALID_CMD,"the option \"%s=%s\" is invalid\n",arg.first.c_str(),arg.second.c_str());
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
							return_msg(ERR_INVALID_CMD,"-e can not be used with multiple files\n");
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
			return_msg(ERR_INVALID_CMD,"the option \"%s\" is invalid\n",arg.first.c_str());
		}
		else
		{
			if(secset&&!newcat->files.empty())
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_INVALID_CMD,"--sec can not be used with multiple files\n");
			}
			if(!arg.second.empty())
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_INVALID_CMD,"the path \"%s=%s\" is invalid\n",arg.first.c_str(),arg.second.c_str());
			}
			int ret=0;
			ph.path=arg.first;
			if(0!=(ret=get_full_path(ctx->pwd,ph.path,fullpath)))
			{
				t_cat_clean_priv(newcat);
				return_msg(ret,"invalid path: \"%s\"\n",ph.path.c_str());
			}
			ph.hfile=fs_open((char*)fullpath.c_str(),FILE_OPEN_EXISTING|FILE_READ);
			if(!VALID(ph.hfile))
			{
				t_cat_clean_priv(newcat);
				return_msg(ERR_FILE_IO,"can not open file: \"%s\" for reading\n",ph.path.c_str());
			}
			newcat->files.push_back(ph);
		}
	}
	if(newcat->files.empty())
	{
		t_cat_clean_priv(newcat);
		return_msg(ERR_INVALID_CMD,"no file specified\n");
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
		return_msg(ret,"cat file \"%s\" error: %s\n",path.c_str(),get_error_desc(ret));
	while(len>zero||infinite)
	{
		uint readlen=(len<once&&!infinite?len.low:buflen);
		if(0!=(ret=fs_read(hfile,buf,readlen,&rdlen)))
			return_msg(ret,"cat file \"%s\" error: %s\n",path.c_str(),get_error_desc(ret));
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
	"and can be either decimal or heximal, with heximal values prefixed by \"0x\".\n"
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
		return_msg(ERR_INVALID_CMD,"The command \"%s\" cannot be used without \"|\"\n",cmd.c_str());
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
					return_msg(ERR_INVALID_CMD,"the option \"%s=%s\" is invalid\n",arg.first.c_str(),arg.second.c_str());
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
			return_msg(ERR_INVALID_CMD,"the option \"%s\" is invalid\n",arg.first.c_str());
		}
		else
		{
			if(file_arg)
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_INVALID_CMD,"can not write to multiple files\n");
			}
			if(!arg.second.empty())
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_INVALID_CMD,"the path \"%s=%s\" is invalid\n",arg.first.c_str(),arg.second.c_str());
			}
			file_arg=true;
			int ret=0;
			newwrite->file.path=arg.first;
			if(0!=(ret=get_full_path(ctx->pwd,newwrite->file.path,fullpath)))
			{
				t_write_clean_priv(newwrite);
				return_msg(ret,"invalid path: \"%s\"\n",arg.first.c_str());
			}
			newwrite->file.hfile=fs_open((char*)fullpath.c_str(),FILE_OPEN_ALWAYS|FILE_READ|FILE_WRITE|FILE_EXCLUSIVE_WRITE);
			if(!VALID(newwrite->file.hfile))
			{
				t_write_clean_priv(newwrite);
				return_msg(ERR_FILE_IO,"can not open file: \"%s\" for writing\n",arg.first.c_str());
			}
		}
	}
	if(!file_arg)
	{
		t_write_clean_priv(newwrite);
		return_msg(ERR_INVALID_CMD,"no file specified\n");
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
		t_output("seek for writing file \"%s\" error: %s\n",writedata->file.path.c_str(),get_error_desc(ret));
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
				t_output("write file \"%s\" error: %s\n",writedata->file.path.c_str(),get_error_desc(ret));
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
	"write binary data into a file, for details, see \"help write\".",
	"Format:\n\t(pre-commands) | write [options] (file-path)\n"
	"NOTE: THIS COMMAND NEEDS TO BE USED WITH \"|\" TO PROVIDE A BINARY STREAM TO BE WRITTEN.\n"
	"The write command writes a stream of binary data into a file, if the file does not exist, "
	"it will be created.\n"
	"The offset to write to the file can be specified as well.\n"
	"Options:\n"
	"--off=[start-offset]\n"
	"\tThis option specifies the offset in count of bytes to start writing. "
	"The start-offset value must be within a signed 64-bit binary integer, "
	"and can be either decimal or heximal, when using a heximal value, it should be prefixed by \"0x\".\n"
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
		return_msg(ERR_INVALID_CMD,"The command \"%s\" cannot be used with \"|\"\n",cmd.c_str());
	for(int i=1;i<(int)args.size();i++)
	{
		const pair<string,string>& arg=args[i];
		if(!arg.second.empty())
			return_msg(ERR_INVALID_CMD,"invalid parameter: \"%s=%s\", quotes needed.\n",arg.first.c_str(),arg.second.c_str());
		if(arg.first.empty())
			continue;
		if(arg.first[0]!='i')
		{
			if(arg.first.size()%2!=0)
				return_msg(ERR_INVALID_CMD,"invalid parameter: \"%s\"\n",arg.first.c_str());
			for(int j=0;j<(int)arg.first.size();j++)
			{
				char c=arg.first[j];
				if(!((c>='0'&&c<='9')
					||(c>='a'&&c<='f')
					||(c>='A'&&c<='F')))
					return_msg(ERR_INVALID_CMD,"invalid parameter: \"%s\"\n",arg.first.c_str());
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
		return_msg(ERR_INVALID_CMD,"The command \"%s\" cannot be used without \"|\"\n",cmd.c_str());
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
					return_msg(ERR_INVALID_CMD,"the option \"%s=%s\" is invalid\n",arg.first.c_str(),arg.second.c_str());
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
		return_msg(ERR_INVALID_CMD,"the option \"%s\" is invalid\n",arg.first.c_str());
	}
	if((newfmt->bshow_ascii||newfmt->bshow_addr)&&newfmt->line_len==0)
	{
		const char* ill_option=newfmt->bshow_ascii?"a":"r";
		fmt_clean_priv(newfmt);
		return_msg(ERR_INVALID_CMD,"--linelen must be specified when using the option -%s\n",ill_option);
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
	"NOTE: THIS COMMAND NEEDS TO BE USED WITH \"|\" TO PROVIDE A BINARY STREAM TO BE DISPLAYED.\n"
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
	bool detect_outer;
	const char* inner;
	string get_inner_path()
	{
		const char* pstr=strrchr(inner,'/');
		return pstr==NULL?"":pstr;
	}
};
static int check_path(cmd_param_st* param,const string& path,string& fullpath,dword* flags=NULL,st_inner_path* inner=NULL)
{
	common_sh_args(param);
	int ret=0;
	if(0!=(ret=get_full_path(ctx->pwd,path,fullpath)))
		return_t_msg(ret,"%s: invalid path\n",path.c_str());
	if(fullpath.empty())
		return_t_msg(ERR_INVALID_PATH,"%s: invalid path\n",path.c_str());
	if(fullpath[0]=='/')
	{
		vector<string> devs;
		uint def=0;
		if(0!=(ret=fs_list_dev(devs,&def)))
			return_t_msg(ret,"%s\n",get_error_desc(ret));
		fullpath=devs[def]+":"+(fullpath=="/"?"":fullpath);
	}
	string strret;
	dword dummy=0;
	dword* pflag=(flags!=NULL?flags:(inner!=NULL?&dummy:NULL));
	if(inner!=NULL)
		inner->detect_outer=false;
	if(0!=(ret=validate_path(fullpath,pflag,NULL,NULL,&strret)))
	{
		if(ret==ERR_FS_FILE_NOT_EXIST&&inner!=NULL)
		{
			int pos=fullpath.rfind('/');
			if(pos!=string::npos)
			{
				string fullpath_outer=fullpath.substr(0,pos);
				if(0==(ret=validate_path(fullpath_outer,flags,NULL,NULL,&strret)))
				{
					inner->detect_outer=true;
					return 0;
				}
			}
		}
		t_output("%s:\n%s",path.c_str(),strret.c_str());
		return ret;
	}
	if(inner!=NULL&&FS_IS_DIR(*pflag))
	{
		string inner_path=inner->get_inner_path();
		if(inner_path.empty())
			return_t_msg(ERR_GENERIC,"%s: cannot move/copy/remove root directory\n",inner->inner);
		inner->detect_outer=true;
		fullpath+=inner_path;
	}
	return 0;
}
static int mv_handler(cmd_param_st* param)
{
	common_sh_args(param);
	if(args.size()!=3)
		return_t_msg(ERR_INVALID_PARAM,"%s: command must take 2 parameters\n",cmd.c_str());
	for(int i=1;i<3;i++)
	{
		if(!args[i].second.empty())
			return_t_msg(ERR_INVALID_PARAM,"%s=%s: invalid command parameter\n",args[i].first.c_str(),args[i].second.c_str());
	}
	string src,dest;
	int ret=0;
	st_inner_path inner={false,src.c_str()};
	if(0!=(ret=check_path(param,args[1].first,src)))
		return_t_msg(ret,"%s: path check failed\n",args[1].first.c_str());
	if(0!=(ret=check_path(param,args[2].first,dest,NULL,&inner)))
		return_t_msg(ret,"%s: path check failed\n",args[2].first.c_str());
	if(!inner.detect_outer)
		return_t_msg(ERR_GENERIC,"the destination path \"%s\" already exists\n",args[2].first.c_str());
	if(dest.size()==src.size())
		return 0;
	if(dest.size()>src.size()&&dest.substr(0,src.size())==src)
		return_t_msg(ERR_GENERIC,"cannot move a directory to its sub-directory\n");
	if(0!=(ret=fs_move((char*)(src.c_str()),(char*)(dest.c_str()))))
		return_t_msg(ret,"%s: %s\n",cmd.c_str(),get_error_desc(ret));
	return 0;
}
DEF_SH_CMD(mv,mv_handler,
	"move a file or a directory to a new path.",
	""
);
static int cp_handler(cmd_param_st* param)
{
	common_sh_args(param);
	return 0;
}
DEF_SH_CMD(cp,cp_handler,
	"copy a file(or files) or a directory(or directories) to a new path.",
	""
);
static int rm_handler(cmd_param_st* param)
{
	common_sh_args(param);
	return 0;
}
DEF_SH_CMD(rm,rm_handler,
	"remove a file(or files) or a directory(or directories).",
	""
);
static int mkdir_handler(cmd_param_st* param)
{
	common_sh_args(param);
	return 0;
}
DEF_SH_CMD(mkdir,mkdir_handler,
	"create a new directory(or directories).",
	""
);
