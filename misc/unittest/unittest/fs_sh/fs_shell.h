#ifndef _FS_SHELL_H_
#define _FS_SHELL_H_
#include "common.h"
#include "fsenv.h"
#include <vector>
#include <string>
using namespace std;
#define FS_CMD_HANDLE_STATE_EXEC 0
#define FS_CMD_HANDLE_STATE_INIT 1
#define FS_CMD_HANDLE_STATE_EXIT 2
#ifdef WIN32
#include "key_def_win.h"
#else
#include "key_def_linux.h"
#endif
#define CMD_PROMPT ">"
#if defined(USE_FS_ENV_VAR)
#define USE_CTX_PRIV
#endif
#ifdef USE_CTX_PRIV
#define init_ctx_priv(ctx) init_ctx_priv_data(ctx)
#define destroy_ctx_priv(ctx) destroy_ctx_priv_data(ctx)
#else
#define init_ctx_priv(ctx)
#define destroy_ctx_priv(ctx)
#endif
#define CTXPRIV_ENVF_DEL 1
struct ctx_priv_data
{
#ifdef USE_FS_ENV_VAR
	struct fsenv_data
	{
		FSEnvSet env_cache;
		dword env_flags;
		fsenv_data()
		{
			env_flags=0;
		}
	}env_data;
#define ctx2env(ctx) ((ctx)->priv->env_data)
#define priv2env(priv) ((priv)->env_data)
#endif
};
struct sh_context
{
	uint c;
	uint icmd;
	uint ilog;
	dword flags;
	ctx_priv_data* priv;
	string cmd;
	string pwd;
	vector<string> logback;
	sh_context()
	{
		c=0;
		icmd=0;
		ilog=0;
		flags=0;
		priv=NULL;
	}
};
typedef int (*fs_cmd_handler)(sh_context*,dword);
inline void print_prompt(sh_context* ctx)
{
	printf("%s%s",ctx->pwd.c_str(),CMD_PROMPT);
}
inline void print_back(uint n)
{
	for(uint i=0;i<n;i++)
		printf("\b");
}
inline void print_blank(uint n)
{
	for(uint i=0;i<n;i++)
		printf(" ");
}
inline void init_ctx_priv_data(sh_context* ctx)
{
	ctx->priv=new ctx_priv_data;
}
inline void destroy_ctx_priv_data(sh_context* ctx)
{
	delete ctx->priv;
	ctx->priv=NULL;
}
uint get_char();
void quit_sh();
void set_cmd_handler(fs_cmd_handler handler);
int run_sh();
#endif