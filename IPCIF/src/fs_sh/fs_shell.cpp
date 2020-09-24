#include "fs_shell.h"
#include <stdio.h>

#define RET_QUIT 1

#define EXIT_CMD "exit"

#define IS_CTRL_KEY(key) (!!((key)&CTRL_KEY))
#define IS_DISP_KEY(key) ((key)>=32&&(key)<=126)

static fs_cmd_handler cmd_handler=NULL;
void set_cmd_handler(fs_cmd_handler handler)
{
	cmd_handler=handler;
}
void erase_current_cmdline(const string& cmd,uint icmd)
{
	print_back(icmd);
	print_blank(cmd.size());
	print_back(cmd.size());
}
bool is_blank(const string& cmd)
{
	if(cmd.empty())
		return true;
	for(int i=0;i<(int)cmd.size();i++)
	{
		if(cmd[i]!=' ')
			return false;
	}
	return true;
}
int execute_cmd(sh_context* ctx)
{
	if(ctx->cmd==EXIT_CMD&&ctx->c!=(uint)'\t')
		return RET_QUIT;
	if(cmd_handler!=NULL)
	{
		int ret=cmd_handler(ctx,FS_CMD_HANDLE_STATE_EXEC);
		return ret==RET_QUIT?ERR_GENERIC:ret;
	}
	return 0;
}
bool process_ctrl_key(sh_context* ctx)
{
	uint c=ctx->c;
	uint& icmd=ctx->icmd;
	uint& ilog=ctx->ilog;
	string& cmd=ctx->cmd;
	vector<string>& logback=ctx->logback;
	bool quit=false;
	switch(c)
	{
	case (uint)'\t':
		execute_cmd(ctx);
		break;
	case VK_LEFT:
		if(icmd>0)
		{
			icmd--;
			print_back(1);
		}
		break;
	case VK_RIGHT:
		if(icmd<cmd.size())
		{
			printf("%c",cmd[icmd]);
			icmd++;
		}
		break;
	case VK_UP:
		if(ilog<logback.size())
		{
			ilog++;
			string hiscmd=logback[logback.size()-ilog];
			erase_current_cmdline(cmd,icmd);
			cmd=hiscmd;
			icmd=cmd.size();
			printf("%s",cmd.c_str());
		}
		break;
	case VK_DOWN:
		if(ilog>0)
		{
			ilog--;
			string hiscmd=(ilog==0?"":logback[logback.size()-ilog]);
			erase_current_cmdline(cmd,icmd);
			cmd=hiscmd;
			icmd=cmd.size();
			printf("%s",cmd.c_str());
		}
		break;
	case VK_BACKSPACE:
#ifdef VK_BACKSPACE_2
	case VK_BACKSPACE_2:
#endif
		if(icmd>0)
		{
			icmd--;
			cmd.erase(cmd.begin()+icmd);
			print_back(1);
			printf("%s",cmd.substr(icmd,cmd.size()-icmd).c_str());
			print_blank(1);
			print_back(cmd.size()-icmd+1);
		}
		break;
	case VK_DELETE:
		if(icmd<cmd.size())
		{
			cmd.erase(cmd.begin()+icmd);
			printf("%s",cmd.substr(icmd,cmd.size()-icmd).c_str());
			print_blank(1);
			print_back(cmd.size()-icmd+1);
		}
		break;
	case VK_RETURN:
		printf("\n");
		if(!is_blank(cmd))
		{
			quit=(RET_QUIT==execute_cmd(ctx));
			if(logback.empty()||logback.back()!=cmd)
				logback.push_back(cmd);
		}
		ilog=0;
		cmd.clear();
		icmd=0;
		if(!quit)
			print_prompt(ctx);
		break;
	}
	return quit;
}
void process_normal_key(sh_context* ctx)
{
	uint c=ctx->c;
	uint& icmd=ctx->icmd;
	string& cmd=ctx->cmd;
	uchar uc=(uchar)c;
	cmd.insert(cmd.begin()+icmd,(char)uc);
	icmd++;
	printf("%c",uc);
	printf("%s",cmd.substr(icmd,cmd.size()-icmd).c_str());
	print_back(cmd.size()-icmd);
}
int run_sh()
{
	sh_context ctx;
	if(cmd_handler!=NULL)
		cmd_handler(&ctx,FS_CMD_HANDLE_STATE_INIT);
	print_prompt(&ctx);
	for(;;)
	{
		ctx.c=get_char();
		if(ctx.c==0)
			break;
		if((!IS_CTRL_KEY(ctx.c))&&IS_DISP_KEY(ctx.c))
		{
			process_normal_key(&ctx);
		}
		else
		{
			if(process_ctrl_key(&ctx))
				break;
		}
	}
	return 0;
}