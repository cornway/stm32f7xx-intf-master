#include <stdint.h>

#ifndef __DEVIO_H__
#define __DEVIO_H__

#include <arch.h>

typedef enum {
    FTYPE_FILE,
    FTYPE_DIR,
} ftype_t;

typedef enum {
    DVAR_FUNC,
    DVAR_INT32,
    DVAR_FLOAT,
    DVAR_STR,
} dvar_obj_t;

typedef int (*dvar_func_t) (void *, void *);

typedef struct {
    void *ptr;
    uint16_t ptrsize;
    uint16_t size;
    dvar_obj_t type;
    uint32_t flags;
} dvar_t;

#define DSEEK_SET 0
#define DSEEK_CUR 1
#define DSEEK_END 2

typedef struct {
    ftype_t type;
    int h;
    char name[128];
    void *ptr;
} fobj_t;

typedef int (*list_clbk_t)(char *name, ftype_t type);

typedef struct {
    list_clbk_t clbk;
    void *user;
} flist_t;

int dev_io_init (void);
int d_open (const char *path, int *hndl, char const * att);
int d_size (int hndl);
int d_tell (int h);
void d_close (int h);
int d_unlink (const char *path);
int d_seek (int handle, int position, uint32_t mode);
int d_eof (int handle);
int d_read (int handle, PACKED void *dst, int count);
char *d_gets (int handle, PACKED char *dst, int count);
char d_getc (int h);
int d_write (int handle, PACKED const void *src, int count);
int d_printf (int handle, char *fmt, ...);
int d_mkdir (const char *path);
int d_opendir (const char *path);
int d_closedir (int dir);
int d_readdir (int dir, fobj_t *fobj);
uint32_t d_time (void);
int d_dirlist (const char *path, flist_t *flist);

int d_dvar_reg (dvar_t *var, const char *name);
int d_dvar_int32 (int32_t *var, const char *name);
int d_dvar_float (float *var, const char *name);
int d_dvar_str (char *str, int len, const char *name);

int d_dvar_rm (const char *name);

#endif
