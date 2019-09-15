#ifndef __BSP_MOD_INT_H__
#define __BSP_MOD_INT_H__

#include <bsp_api.h>
#include <heap.h>

typedef struct bsp_mod_api_s {
    bspdev_t dev;
    void *(*insert) (const bsp_heap_api_t *, const char *, const char *);
    int (*remove) (const char *);
    int (*probe) (const char *);
    int (*register_api) (const char *, const void *, int);
    const void *(*get_api) (const char *, int *);
} bsp_mod_api_t;

#define BSP_MOD_API(func) ((bsp_mod_api_t *)(g_bspapi->mod))->func

#if BSP_INDIR_API

#define bspmod_init              BSP_MOD_API(dev.init)
#define bspmod_deinit            BSP_MOD_API(dev.deinit)
#define bspmod_conf              BSP_MOD_API(dev.conf)
#define bspmod_info              BSP_MOD_API(dev.info)
#define bspmod_priv              BSP_MOD_API(dev.priv)
#define bspmod_insert            BSP_MOD_API(insert)
#define bspmod_remove            BSP_MOD_API(remove)
#define bspmod_probe             BSP_MOD_API(probe)

#else /*BSP_INDIR_API*/

void *bspmod_insert (const bsp_heap_api_t *, const char *, const char *);
int bspmod_remove (void (*_free) (void *), const char *);
int bspmod_probe (const char *name, int argc, const char **argv);
void *bspmod_get_sym (void *_mod, const char *name);

#define bspmod_get_func(mod, func) \
    bspmod_get_sym(_mod, #func)

#endif /*BSP_INDIR_API*/

#endif /*__BSP_MOD_INT_H__*/

