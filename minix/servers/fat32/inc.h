#ifndef MINIX_SERVERS_FAT32_INC_H_
#define MINIX_SERVERS_FAT32_INC_H_

/* Header file including all needed system headers. */

#define _SYSTEM		1	/* get OK and negative error codes */

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <limits.h>
#include <errno.h>
#include <regex.h>

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/syslib.h>
#include <minix/sysinfo.h>
#include <minix/sysutil.h>
#include <minix/bitmap.h>
#include <minix/rs.h>

#include <sys/errno.h>

#include <stdlib.h>
#include <string.h>

#include <signal.h>
#ifdef __cplusplus
}
#endif


#include "proto.h"

#endif