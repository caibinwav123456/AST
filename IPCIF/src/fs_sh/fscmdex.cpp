#include "sh_cmd_fs.h"
//echo cat fmt hex write cp mv rm mkdir setlen touch
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