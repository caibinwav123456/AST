#ifndef _DEFINES_H_
#define _DEFINES_H_
#include "common.h"

#define CONFIG_SECTION_VARIABLES "variables"
#define DEFAULT_CONFIG_SECTION CONFIG_SECTION_VARIABLES
#define CONFIG_SECTION_EXEC  "executables"
#define CFG_FILE_PATH "config.cfg"

#define CFG_TAG_EXEC_FILE "file"
#define CFG_TAG_EXEC_CMDLINE "cmdline"
#define CFG_TAG_EXEC_ID "id"
#define CFG_TAG_EXEC_UNIQUE "unique"
#define CFG_TAG_EXEC_LDIR "use_exec_dir_as_cur_dir"
#define CFG_TAG_EXEC_LAUNCHER "is_launcher"
#define CFG_TAG_EXEC_MANAGER "is_manager"
#define CFG_TAG_EXEC_MANAGED "is_managed"
#define CFG_TAG_EXEC_AMBIG "ambiguous"
#define CFG_TAG_EXEC_IF "if%d"
#define CFG_TAG_EXEC_IF_CNT "if%d_cnt"
#define CFG_TAG_EXEC_IF_USAGE "if%d_usage"
#define CFG_TAG_EXEC_IF_PRIOR "if%d_prior"

#define CFG_IF_USAGE_CMD "cmd"
#define CFG_IF_USAGE_STORAGE "storage"
#define CFG_IF_USAGE_NET "net"

#endif