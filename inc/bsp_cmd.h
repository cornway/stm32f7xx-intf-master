#ifndef __BSP_CMD_H__
#define __BSP_CMD_H__

#include "../int/bsp_cmd_int.h"

typedef struct bsp_cmd_api_s {
    bspdev_t dev;
    int (*var_reg) (cmdvar_t *, const char *);
    int (*var_int32) (int32_t *, const char *);
    int (*var_float) (float *, const char *);
    int (*var_str) (char *, int, const char *);
    int (*var_func) (cmd_func_t, const char *);
    int (*var_rm) (const char *);
    int (*exec) (const char *, int);
    void (*tickle) (void);
} bsp_cmd_api_t;

#define BSP_CMD_API(func) ((bsp_cmd_api_t *)(g_bspapi->cmd))->func

#if BSP_INDIR_API

#define cmd_register_var      BSP_CMD_API(var_reg)
#define cmd_register_i32      BSP_CMD_API(var_int32)
#define cmd_register_float    BSP_CMD_API(var_float)
#define cmd_register_str      BSP_CMD_API(var_str)
#define cmd_register_func     BSP_CMD_API(var_func)
#define cmd_execute            BSP_CMD_API(exec)
#define cmd_tickle             BSP_CMD_API(tickle)

#else /*BSP_INDIR_API*/

int cmd_register_var (cmdvar_t *var, const char *name);
int cmd_register_i32 (int32_t *var, const char *name);
int cmd_register_float (float *var, const char *name);
int cmd_register_str (char *str, int len, const char *name);
int cmd_register_func (cmd_func_t func, const char *name);
void cmd_tickle (void);

#endif /*BSP_INDIR_API*/


#endif /*__BSP_CMD_H__*/

