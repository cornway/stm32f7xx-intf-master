
#include <stm32f7xx_it.h>
#include <stm32f769i_discovery_lcd.h>

#include <lcd_int.h>
#include <nvic.h>

#include <misc_utils.h>
#include <bsp_sys.h>

enum {
    V_STATE_IDLE,
    V_STATE_QCOPY,
    V_STATE_COPYFAST,
    V_STATE_MAX,
};

typedef struct screen_hal_ctxt_s {
    lcd_wincfg_t *lcd_cfg;
    void *hal_dma;
    void *hal_cfg;
    void *hal_ltdc;
    copybuf_t *bufq;
    uint8_t busy;
    uint8_t poll;
    uint8_t state;
    uint8_t waitreload;
} screen_hal_ctxt_t;

#define GET_VHAL_CTXT(cfg) ((screen_hal_ctxt_t *)((lcd_wincfg_t *)(cfg))->hal_ctxt)
#define GET_VHAL_LTDC(cfg) ((LTDC_HandleTypeDef *)GET_VHAL_CTXT(cfg)->hal_ltdc)
#define GET_VHAL_DMA2D(cfg) ((DMA2D_HandleTypeDef *)GET_VHAL_CTXT(cfg)->hal_dma)
#define GET_VHAL_HALCFG(cfg) ((LCD_LayerCfgTypeDef *)GET_VHAL_CTXT(cfg)->hal_cfg)
#define VHAL_PIX_FMT(cfg, num) (GET_VHAL_LTDC(cfg)->LayerCfg[num].PixelFormat)

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d);

lcd_layers_t screen_hal_set_layer (lcd_wincfg_t *cfg)
{
    lcd_layers_t nextlay;

    switch (cfg->ready_lay_idx) {
        case LCD_BACKGROUND:
            BSP_LCD_SelectLayer(LCD_BACKGROUND);
            BSP_LCD_SetTransparency(LCD_FOREGROUND, GFX_OPAQUE);
            BSP_LCD_SetTransparency(LCD_BACKGROUND, GFX_TRANSPARENT);
            nextlay = LCD_BACKGROUND;
        break;
        case LCD_FOREGROUND:
            BSP_LCD_SelectLayer(LCD_FOREGROUND);
            BSP_LCD_SetTransparency(LCD_BACKGROUND, GFX_OPAQUE);
            BSP_LCD_SetTransparency(LCD_FOREGROUND, GFX_TRANSPARENT);
            nextlay = LCD_FOREGROUND;
        break;
        default:
            assert(0);
        break;
    }

    return nextlay;
}

static screen_hal_ctxt_t screen_hal_ctxt = {{0}};

static const uint32_t dma2d_color_mode2out_map[] =
{
    [GFX_COLOR_MODE_CLUT] = DMA2D_OUTPUT_ARGB8888,
    [GFX_COLOR_MODE_RGB565] = DMA2D_OUTPUT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = DMA2D_OUTPUT_ARGB8888,
};

static const uint32_t dma2d_color_mode2in_map[] =
{
    [GFX_COLOR_MODE_CLUT] = DMA2D_INPUT_L8,
    [GFX_COLOR_MODE_RGB565] = DMA2D_INPUT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = DMA2D_INPUT_ARGB8888,
};

static const uint32_t screen_mode2fmt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = LTDC_PIXEL_FORMAT_L8,
    [GFX_COLOR_MODE_RGB565] = LTDC_PIXEL_FORMAT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = LTDC_PIXEL_FORMAT_ARGB8888,
};

static void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt);
static void screen_copybuf_split (screen_hal_ctxt_t *ctxt, copybuf_t *buf, int parts);
static int screen_hal_copy_next (screen_hal_ctxt_t *ctxt);

void screen_hal_set_clut (lcd_wincfg_t *cfg, void *_buf, int size, int layer)
{
    if (VHAL_PIX_FMT(cfg, layer) != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]) {
        return;
    }
    HAL_LTDC_ConfigCLUT(GET_VHAL_LTDC(cfg), (uint32_t *)_buf, size, layer);
    HAL_LTDC_EnableCLUT(GET_VHAL_LTDC(cfg), layer);
}

int screen_hal_set_keying (lcd_wincfg_t *cfg, uint32_t color, int layer)
{
    BSP_LCD_SetColorKeying(layer, color);
    return 0;
}

int screen_hal_init (int init)
{
    uint32_t status;

    d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
    if (init) {

        status = BSP_LCD_Init();
        assert(!status);

        bsp_lcd_width = BSP_LCD_GetXSize();
        bsp_lcd_height = BSP_LCD_GetYSize();

        BSP_LCD_SetBrightness(100);
    } else {
        BSP_LCD_SetBrightness(0);
        BSP_LCD_DeInitEx();
        HAL_Delay(1000);
    }
    return 0;
}

void screen_hal_attach (lcd_wincfg_t *cfg)
{
extern LTDC_HandleTypeDef hltdc_discovery;
    static DMA2D_HandleTypeDef dma2d;
    static LCD_LayerCfgTypeDef hal_cfg;

    cfg->hal_ctxt = &screen_hal_ctxt;
    GET_VHAL_CTXT(cfg)->hal_ltdc = &hltdc_discovery;
    GET_VHAL_CTXT(cfg)->hal_dma = &dma2d;
    GET_VHAL_CTXT(cfg)->hal_cfg = &hal_cfg;
    screen_hal_ctxt.lcd_cfg = cfg;
}

void *
screen_hal_set_config (lcd_wincfg_t *cfg, int x, int y, int w, int h, uint8_t colormode)
{
    int layer;
    LCD_LayerCfgTypeDef *Layercfg;

    screen_hal_attach(cfg);

    Layercfg = GET_VHAL_HALCFG(cfg);
    /* Layer Init */
    Layercfg->WindowX0 = x;
    Layercfg->WindowX1 = x + w;
    Layercfg->WindowY0 = y;
    Layercfg->WindowY1 = y + h;
    Layercfg->PixelFormat = screen_mode2fmt_map[colormode];
    Layercfg->Alpha = 255;
    Layercfg->Alpha0 = 0;
    Layercfg->Backcolor.Blue = 0;
    Layercfg->Backcolor.Green = 0;
    Layercfg->Backcolor.Red = 0;
    Layercfg->BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg->BlendingFactor2 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg->ImageWidth = w;
    Layercfg->ImageHeight = h;

    for (layer = 0; layer < cfg->config.laynum; layer++) {
        Layercfg->FBStartAdress = (uint32_t)cfg->lay_mem[layer];
        HAL_LTDC_ConfigLayer(GET_VHAL_LTDC(cfg), Layercfg, layer);
    }

    return Layercfg;
}

static inline void __screen_hal_vsync (lcd_wincfg_t *cfg)
{
    if (GET_VHAL_CTXT(lcd_active_cfg)->poll) {
        while ((LTDC->CDSR & LTDC_CDSR_VSYNCS)) {}
    } else {
        while (GET_VHAL_CTXT(lcd_active_cfg)->waitreload) {
            HAL_Delay(1);
        }
    }
}

void screen_hal_sync (lcd_wincfg_t *cfg, int wait)
{
    while (GET_VHAL_CTXT(cfg)->state != V_STATE_IDLE) {
        HAL_Delay(1);
    }
    if (wait) {
        __screen_hal_vsync(cfg);
    }
}

static int screen_update_direct (lcd_wincfg_t *cfg, screen_t *psrc)
{
    copybuf_t buf = {NULL};
    screen_t *dest = &buf.dest, *src = &buf.src;

    if (!psrc) {
        psrc = src;
        src->buf = cfg->lay_mem[layer_switch[cfg->ready_lay_idx]];
        src->x = 0;
        src->y = 0;
        src->width = cfg->w;
        src->height = cfg->h;
        src->colormode = cfg->config.colormode;
    }
    dest->buf = cfg->lay_mem[cfg->ready_lay_idx];
    dest->x = 0;
    dest->y = 0;
    dest->width = cfg->w;
    dest->height = cfg->h;
    dest->colormode = cfg->config.colormode;
    if (GET_VHAL_CTXT(cfg)->poll) {
        return screen_hal_copy_m2m(cfg, &buf, screen_mode2pixdeep[dest->colormode]);
    } else {
        irqmask_t irq;
        int ret;

        irq_save(&irq);
        if (GET_VHAL_CTXT(cfg)->bufq) {
            return 0;
        }
        screen_copybuf_split(GET_VHAL_CTXT(cfg), &buf, 4);
        ret = screen_hal_copy_next(GET_VHAL_CTXT(cfg));
        irq_restore(irq);
        return ret;
    }
}

static DMA2D_HandleTypeDef *
__screen_hal_copy_setup_M2M
(
    screen_hal_ctxt_t *ctxt,
    uint8_t dst_colormode,
    uint8_t src_colormode,
    uint8_t src_alpha,
    int src_width,
    int dest_wtotal,
    int src_wtotal
)
{
    DMA2D_HandleTypeDef *hdma2d = GET_VHAL_DMA2D(ctxt->lcd_cfg);
    const int layid = 1;

    d_memzero(&hdma2d->Init, sizeof(hdma2d->Init));

    hdma2d->Init.Mode         = DMA2D_M2M;
    hdma2d->Init.ColorMode    = dma2d_color_mode2out_map[dst_colormode];
    hdma2d->Init.OutputOffset = dest_wtotal - src_width;
    hdma2d->Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d->Init.RedBlueSwap   = DMA2D_RB_REGULAR;

    hdma2d->XferCpltCallback = DMA2D_XferCpltCallback;

    hdma2d->LayerCfg[layid].AlphaMode = DMA2D_COMBINE_ALPHA;
    hdma2d->LayerCfg[layid].InputAlpha = src_alpha;
    hdma2d->LayerCfg[layid].InputColorMode = dma2d_color_mode2in_map[src_colormode];
    hdma2d->LayerCfg[layid].InputOffset = src_wtotal - src_width;
    hdma2d->LayerCfg[layid].RedBlueSwap = DMA2D_RB_REGULAR;
    hdma2d->LayerCfg[layid].AlphaInverted = DMA2D_REGULAR_ALPHA;

    hdma2d->Instance = DMA2D;

    if(HAL_DMA2D_Init(hdma2d) != HAL_OK) {
        return NULL;
    }
    if (HAL_DMA2D_ConfigLayer(hdma2d, layid) != HAL_OK) {
        return NULL;
    }

    return hdma2d;
}

static inline void
__DMA2D_SetConfig_M2M (DMA2D_HandleTypeDef *hdma2d, uint32_t pdata,
                               uint32_t DstAddress, uint32_t Width, uint32_t Height)
{
    MODIFY_REG(hdma2d->Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (Height| (Width << DMA2D_NLR_PL_Pos))); 
    WRITE_REG(hdma2d->Instance->OMAR, DstAddress);
    WRITE_REG(hdma2d->Instance->FGMAR, pdata);
}

static inline HAL_StatusTypeDef
__HAL_DMA2D_Start_IT (DMA2D_HandleTypeDef *hdma2d, uint32_t pdata,
                             uint32_t DstAddress, uint32_t Width,  uint32_t Height)
{
  __HAL_LOCK(hdma2d);

  hdma2d->State = HAL_DMA2D_STATE_BUSY;

  __DMA2D_SetConfig_M2M(hdma2d, pdata, DstAddress, Width, Height);

  /* Enable the transfer complete, transfer error and configuration error interrupts */
  __HAL_DMA2D_ENABLE_IT(hdma2d, DMA2D_IT_TC|DMA2D_IT_TE|DMA2D_IT_CE);

  /* Enable the Peripheral */
  __HAL_DMA2D_ENABLE(hdma2d);

  return HAL_OK;
}

static inline int 
screen_hal_copy_start
(
    lcd_wincfg_t *cfg,
    uint32_t width,
    uint32_t height,
    void *dptr,
    void *sptr
)
{
    DMA2D_HandleTypeDef *hdma2d = GET_VHAL_DMA2D(cfg);
    HAL_StatusTypeDef status = HAL_OK;

    if (GET_VHAL_CTXT(cfg)->poll) {
        if (HAL_DMA2D_Start(hdma2d, (uint32_t)sptr, (uint32_t)dptr, width, height) != HAL_OK) {
            return -1;
        }
        status = HAL_DMA2D_PollForTransfer(hdma2d, GET_VHAL_CTXT(cfg)->poll);
        GET_VHAL_CTXT(cfg)->state = V_STATE_IDLE;
    } else {
        if (__HAL_DMA2D_Start_IT(hdma2d, (uint32_t)sptr, (uint32_t)dptr, width, height) != HAL_OK) {
            return -1;
        }
    }
    if (status != HAL_OK) {
        return -1;
    }
    return 0;
}

static inline void *
__screen_2_ptr (screen_t *s, uint8_t pixbytes)
{
    return (void *)((uint32_t)s->buf + (s->y * s->width + s->x) * pixbytes);
}

static inline void *
__gfx_2_ptr (gfx_2d_buf_t *buf, uint8_t pixbytes)
{
    return (void *)((uint32_t)buf->buf + (buf->y * buf->wtotal + buf->x) * pixbytes);
}

static inline void
__screen_check (lcd_wincfg_t *cfg, screen_t *s)
{
    if (s->colormode == GFX_COLOR_MODE_AUTO) {
        s->colormode = cfg->config.colormode;
    }
}

int screen_hal_copy_m2m (lcd_wincfg_t *cfg, copybuf_t *copybuf, uint8_t pix_bytes)
{
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;

    void *dptr = __screen_2_ptr(dest, pix_bytes);
    void *sptr = __screen_2_ptr(src,  pix_bytes);

    __screen_check(cfg, dest);
    __screen_check(cfg, src);

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), dest->colormode, src->colormode,
                                    src->alpha, src->width, dest->width, src->width);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, src->width, src->height, dptr, sptr);
}

int screen_gfx8888_copy (lcd_wincfg_t *cfg, gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    void *dptr = __gfx_2_ptr(dest, 4);
    void *sptr = __gfx_2_ptr(src, 4);

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), GFX_COLOR_MODE_RGBA8888, GFX_COLOR_MODE_RGBA8888,
                                    0xff, src->w, dest->wtotal, src->wtotal);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, src->w, src->h, dptr, sptr);
}

int screen_gfx8_copy_line (lcd_wincfg_t *cfg, void *dest, void *src, int w)
{
    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), cfg->config.colormode, cfg->config.colormode,
                                    0xff, w, w, w);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, w, 1, dest, src);
}

typedef struct {
    copybuf_t copybuf;
    void *dptr, *sptr;
    uint16_t dest_leg;
    uint16_t src_leg;
    uint16_t copycnt;
    uint16_t copydone;
    uint8_t interleave;
    uint8_t mode;
} fastline_t;

#define M_INTERLEAVE 0x1

static fastline_t fastline_h8;

int screen_hal_scale_h8_2x2 (lcd_wincfg_t *cfg, copybuf_t *copybuf, int interleave)
{
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;
    int dw = dest->width, sw = src->width;
    const int pix_size = 1;

    void *dptr;
    void *sptr;

    copybuf->next = NULL;
    dest->width = 1;
    src->width = 1;
    src->y = 0;
    dest->y = 0;
    fastline_h8.copybuf = *copybuf;
    fastline_h8.dest_leg = dw;
    fastline_h8.src_leg = sw;
    fastline_h8.copydone = 0;
    if (interleave) {
        fastline_h8.mode |= M_INTERLEAVE;
        fastline_h8.copycnt = dw;
        fastline_h8.interleave = 1 - fastline_h8.interleave;
    } else {
        fastline_h8.copycnt = dw * 2;
    }

    dptr = __screen_2_ptr(dest, pix_size);
    sptr = __screen_2_ptr(src, pix_size);

    fastline_h8.dptr = dptr;
    fastline_h8.sptr = sptr;

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), GFX_COLOR_MODE_CLUT, GFX_COLOR_MODE_CLUT,
                            0xff, src->width, dw * 2, sw);

    GET_VHAL_CTXT(cfg)->state = V_STATE_COPYFAST;

    return screen_hal_copy_start(cfg, src->width, src->height, dptr, sptr);
}

static inline int
screen_hal_copy_h8_next (screen_hal_ctxt_t *ctxt)
{
    uint32_t dptr_off = 0;
    screen_t *dest = &fastline_h8.copybuf.dest;
    screen_t *src = &fastline_h8.copybuf.src;

    if (fastline_h8.copycnt == 0) {
        ctxt->state = V_STATE_IDLE;
        return 0;
    }

    fastline_h8.copycnt--;
    fastline_h8.copydone++;

    if (fastline_h8.mode & M_INTERLEAVE) {
        if (fastline_h8.interleave) {
            dptr_off = fastline_h8.dest_leg;
        }
        if ((fastline_h8.copydone & 0x1) == 0) {
            src->x++;
            fastline_h8.sptr = (void *)((uint32_t)fastline_h8.sptr + 1);
        }
        fastline_h8.dptr = (void *)((uint32_t)fastline_h8.dptr + 1);
    } else switch (fastline_h8.copydone & 0x3) {
        case 0x3:
        case 0x1:
            dptr_off = fastline_h8.dest_leg;
        break;
        case 0x2:
            src->x++;
            fastline_h8.sptr = (void *)((uint32_t)fastline_h8.sptr + 1);
        case 0x0:
            dest->y = 0;
            dest->x++;
            fastline_h8.dptr = (void *)((uint32_t)fastline_h8.dptr + 1);
        break;
    }

    return screen_hal_copy_start(ctxt->lcd_cfg, src->width, src->height,
                                (void *)((uint32_t)fastline_h8.dptr + dptr_off),
                                fastline_h8.sptr);
}

static int
screen_hal_copy_next (screen_hal_ctxt_t *ctxt)
{
    copybuf_t *bufq = ctxt->bufq;
    copybuf_t *buf;
    int ret;

    if (!bufq) {
        return 0;
    }
    buf = bufq;
    bufq = bufq->next;
    ret = screen_hal_copy_m2m(ctxt->lcd_cfg, buf, screen_mode2pixdeep[ctxt->lcd_cfg->config.colormode]);
    ctxt->lcd_cfg->config.alloc.free(buf);
    ctxt->bufq = bufq;
    return ret;
}

static copybuf_t *
screen_hal_copybuf_alloc (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src)
{
    copybuf_t *buf = ctxt->lcd_cfg->config.alloc.malloc(sizeof(*buf));

    d_memcpy(&buf->dest, dest, sizeof(buf->dest));
    d_memcpy(&buf->src, src, sizeof(buf->src));

    buf->next = ctxt->bufq;
    ctxt->bufq = buf;
    return buf;
}

static inline void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt)
{
    switch (ctxt->state) {

        case V_STATE_QCOPY:
                if (ctxt->bufq) {
                    screen_hal_copy_next(ctxt);
                    break;
                }
                GET_VHAL_CTXT(ctxt->lcd_cfg)->state = V_STATE_IDLE;
        break;
        case V_STATE_COPYFAST:
                screen_hal_copy_h8_next(ctxt);
        break;
        default:
        break;
    }
}

static void screen_copybuf_split (screen_hal_ctxt_t *ctxt, copybuf_t *buf, int parts)
{
    screen_t dest = buf->dest, src = buf->src;
    int h, i;

    h = src.height / parts;
    src.height = h;
    dest.height = h;

    for (i = 0; i < parts; i++) {
        screen_hal_copybuf_alloc(ctxt, &dest, &src);
        dest.y += h;
        src.y += h;
    }
    parts = src.height % parts;
    if (parts) {
        dest.height = parts;
        src.height = parts;
        screen_hal_copybuf_alloc(ctxt, &dest, &src);
    }
}

void DMA2D_IRQHandler(void)
{
    HAL_DMA2D_IRQHandler(GET_VHAL_DMA2D(lcd_active_cfg));
    GET_VHAL_CTXT(lcd_active_cfg)->busy = 0;
}

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d)
{
    if (GET_VHAL_DMA2D(lcd_active_cfg) == hdma2d) {
        screen_dma2d_irq_hdlr(GET_VHAL_CTXT(lcd_active_cfg));
    }
}

void BSP_LCD_LTDC_IRQHandler (void)
{
    HAL_LTDC_IRQHandler(GET_VHAL_LTDC(lcd_active_cfg));
}

void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    GET_VHAL_CTXT(lcd_active_cfg)->waitreload = 0;
}

