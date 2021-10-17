// sendcmd.cpp : Defines the entry point for the console application.
//
#include "common.h"
#include "interface.h"
#include "utility.h"
#include "common_request.h"
#include <stdio.h>
#include <assert.h>
#include <string>
#include <vector>
using namespace std;
string user;
string if_id,if_cmd;
vector<string> if_args;
uint cmdcode[]={CMD_EXIT};
string cmdstr[]={"exit"};
bool parse_args(int argc, char** argv)
{
	for(int i=1;i<argc;i++)
	{
		if(strlen(argv[i])>0&&argv[i][0]=='-')
		{
			if(strcmp(argv[i],"-u")==0)
			{
				i++;
				if(i<argc)
				{
					user=argv[i];
				}
			}
			else
			{
				printf("Unknown option: %s\n", argv[i]);
				return false;
			}
		}
		else
		{
			if(if_id=="")
				if_id=argv[i];
			else if(if_cmd=="")
				if_cmd=argv[i];
			else
				if_args.push_back(string(argv[i]));
		}
	}
	if(if_id==""||if_cmd=="")
		return false;
	return true;
}
void print_usage()
{
	puts("\nsendcmd - send a command to a particular interface.\n\n"
		"Usage:\n"
		"\nsendcmd (interface_id) (command_string) [arg1] [arg2]... [options]\n\n"
		"interface_id: the identifier string of the interface.\n\n"
		"command_string: the string specifying the command.\n"
		"Valid commands are:\n\n"
		"exit\n\n"
		"options: specify extra options.\n"
		"Valid options are:\n\n"
		"-u (user)\n"
		"Specify the user of the interface,\n\n"
		"the default user is CaiBin.\n");
}
int request_cmd(uint cmd)
{
	int ret=0;
	switch(cmd)
	{
	case CMD_EXIT:
		return send_cmd_exit(NULL, const_cast<char*>(if_id.c_str()), const_cast<char*>(user.c_str()));
	}
	return ERR_GENERIC;
}
int main(int argc, char** argv)
{
	int ret=0;
	if(0!=(ret=mainly_initial()))
	{
		printf("init failed.");
		return ret;
	}
	if(argc==1)
	{
		print_usage();
		goto end;
	}
	user=get_if_user();
	if(!parse_args(argc, argv))
	{
		printf("bad format.\n");
		ret=ERR_INVALID_PARAM;
		goto end;
	}
	assert(sizeof(cmdcode)/sizeof(uint)==sizeof(cmdstr)/sizeof(string));
	ret=ERR_GENERIC;
	for(int i=0;i<sizeof(cmdcode)/sizeof(uint);i++)
	{
		if(cmdstr[i]==if_cmd)
		{
			if(0!=(ret=request_cmd(cmdcode[i])))
				printf("Command failed.\nError code: %d, %s\n",ret,get_error_desc(ret));
			goto end;
		}
	}
	printf("command \"%s\" not valid\n",if_cmd.c_str());
end:
	mainly_exit();
	return ret;
}
