
#include <string.h>
#include <debug.h>
#include <boot_int.h>
#include <bsp_mod.h>
#include <bsp_sys.h>

#define MOD_MAX_NAME 24

typedef struct bspmod_s {
    struct bspmod_s *next;
    exec_desc_t bin;

    const void *api;
    int apisize;
    bsp_heap_api_t *heap;
    char name[MOD_MAX_NAME];
} bspmod_t;

typedef struct {
    bspmod_t *head;
    int elements;
} bspmod_list_t;

static bspmod_list_t bspmod_list = {0};

static int bspmod_link (bspmod_t *mod)
{
    mod->next = bspmod_list.head;
    bspmod_list.head = mod;
    return bspmod_list.elements++;
}

static void bspmod_unlink (bspmod_t *modrm)
{
    bspmod_t *mod = bspmod_list.head, *prev = NULL;

    while (mod) {

        if (mod == modrm) {
            if (prev) {
                prev->next = modrm->next;
            } else {
                bspmod_list.head = modrm->next;
            }
            bspmod_list.elements--;
            break;
        }

        prev = mod;
        mod = mod->next;
    }
}

static bspmod_t *bspmod_search_mod (const char *name)
{
    bspmod_t *mod = bspmod_list.head;

    while (mod) {
        if (strcmp(mod->name, name) == 0) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

static d_bool bspmod_check_mod_allowed (bspmod_t *modchk)
{
    bspmod_t *mod = bspmod_list.head;

    while (mod) {
        if (mod->bin.parm.entrypoint == modchk->bin.parm.entrypoint) {
            return d_false;
        }
        mod = mod->next;
    }
    return d_true;
}

void *bspmod_insert (const bsp_heap_api_t *heap, const char *path, const char *name)
{
    bspmod_t *mod;
    exec_desc_t *bin;
    bsp_exec_file_type_t bintype;
    void *rawptr;
    int err = -1;

    mod = bspmod_search_mod(name);
    if (mod) {
        return NULL;
    }

    bintype = bsp_bin_file_fmt_supported(name);

    if (bintype != BIN_FILE) {
        return NULL;
    }

    mod = heap_api_malloc(heap, sizeof(*mod));
    assert(mod);

    bin = bsp_setup_bin_desc(path, &mod->bin, path, name, bintype);

    if (!bin || !bspmod_check_mod_allowed(mod)) {
        heap_api_free(heap, mod);
        return NULL;
    }

    rawptr = bres_cache_file_2_mem(heap, bin->path, (int *)&bin->parm.size);

    if (rawptr) {
        err = 0;
        if (!bhal_bin_check_in_mem(NULL, (arch_word_t *)bin->parm.progaddr, rawptr, bin->parm.size)) {
            err = bhal_bin_2_mem_load(NULL, (arch_word_t *)bin->parm.progaddr,
                                     rawptr, bin->parm.size);
        }
    }
    if (err < 0) {
        heap_api_free(heap, mod);
        return NULL;
    }
    mod->heap = (bsp_heap_api_t *)heap;

    bspmod_link(mod);
    heap_api_free(heap, rawptr);

    return mod;
}

int bspmod_remove (const char *name)
{
    bspmod_t *mod;

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    bspmod_unlink(mod);
    heap_api_free(mod->heap, mod);
    return 0;
}

int bspmod_probe (const char *name)
{
    bspmod_t *mod;

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    return bhal_execute_module(mod->bin.parm.entrypoint);
}

/*must be used only within 'module' code*/
int bspmod_register_api (const char *name, const void *api, int apisize)
{
    bspmod_t *mod;

    assert(EXEC_REGION_MODULE())

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    mod->api = api;
    mod->apisize = apisize;
    return 0;
}

const void *bspmod_get_api (const char *name, int *apisize)
{
    bspmod_t *mod;

    assert(EXEC_REGION_APP());

    mod = bspmod_search_mod(name);
    if (!mod) {
        *apisize = 0;
        return NULL;
    }
    *apisize = mod->apisize;
    return mod->api;
}
