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
#define CFG_TAG_EXEC_TYPE "type"
#define CFG_TAG_EXEC_TYPE_LAUNCHER "launcher"
#define CFG_TAG_EXEC_TYPE_MANAGER "manager"
#define CFG_TAG_EXEC_TYPE_MANAGED "managed"
#define CFG_TAG_EXEC_TYPE_TOOL "tool"
#define CFG_TAG_EXEC_LOG "log"
#define CFG_TAG_EXEC_AMBIG "ambiguous"
#define CFG_TAG_EXEC_IF "if%d"
#define CFG_TAG_EXEC_IF_CNT "if%d_cnt"
#define CFG_TAG_EXEC_IF_USAGE "if%d_usage"
#define CFG_TAG_EXEC_IF_PRIOR "if%d_prior"

#define CFG_IF_USAGE_CMD "cmd"
#define CFG_IF_USAGE_STORAGE "storage"
#define CFG_IF_USAGE_NET "net"

#define CFG_SECTION_IF_INFO "if_info_%s"

#define CFG_IF_STO_MOD_NAME "mod_name"
#define CFG_IF_STO_DRV_NAME "drv_name"
#define CFG_IF_STO_MOUNT_CMD "mount_cmd"
#define CFG_IF_STO_FORMAT_CMD "format_cmd"

#ifdef WIN32
#define DEF_STO_MOD_NAME "fsdrv.dll"
#else
#define DEF_STO_MOD_NAME "libfsdrv.so"
#endif
#define STO_GET_INTF_FUNC get_storage_drv_interface
#define STO_GET_INTF_FUNC_NAME "get_storage_drv_interface"

#endif