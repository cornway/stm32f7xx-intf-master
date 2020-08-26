/**
  ******************************************************************************
  * @file    stm32h747i_discovery_lcd.c
  * @author  MCD Application Team
  * @brief   This file includes the driver for Liquid Crystal Display (LCD) module
  *          mounted on STM32H747I_DISCO board.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/*  How To use this driver:
  --------------------------
   - This driver is used to drive directly a LCD TFT using the DSI interface.
     The following IPs are implied : DSI Host IP block working in conjunction to the
	 LTDC controller.
   - This driver is linked by construction to LCD KoD mounted on MB1166 daughter board.
   - This driver is also used to drive monitors using HDMI interface.

  Driver description:
  ---------------------
   + Initialization steps:
     o Initialize the LCD in default mode using the BSP_LCD_Init() function with the
       following settings:
        - DSI is configured in video mode
        - Pixelformat : LCD_PIXEL_FORMAT_RBG888
        - Orientation : LCD_ORIENTATION_LANDSCAPE.
        - Width       : LCD_DEFAULT_WIDTH (800)
        - Height      : LCD_DEFAULT_HEIGHT(480)
       The default LTDC layer configured is layer 0.
       BSP_LCD_Init() includes DSI, LTDC, LTDC Layer and clock configurations by calling:
        - MX_LTDC_ClockConfig()
        - MX_LTDC_Init()
        - MX_DSIHOST_DSI_Init()
        - MX_LTDC_ConfigLayer()

     o Initialize the LCD with required parameters using the BSP_LCD_InitEx() function.
       To initialize DSI in command mode, user have to override MX_DSIHOST_DSI_Init(), weak function,
       content at application level.

     o Initialize the display with HDMI using BSP_LCD_InitHDMI(). Two display formats
       are supported: HDMI_FORMAT_720_480 or HDMI_FORMAT_720_576

     o Select the LCD layer to be used using the BSP_LCD_SelectLayer() function.
     o Enable the LCD display using the BSP_LCD_DisplayOn() function.
     o Disable the LCD display using the BSP_LCD_DisplayOff() function.
     o Set the display brightness using the BSP_LCD_SetBrightness() function.
     o Get the display brightness using the BSP_LCD_GetBrightness() function.
     o Write a pixel to the LCD memory using the BSP_LCD_WritePixel() function.
     o Read a pixel from the LCD memory using the BSP_LCD_ReadPixel() function.
     o Draw an horizontal line using the BSP_LCD_DrawHLine() function.
     o Draw a vertical line using the BSP_LCD_DrawVLine() function.
     o Draw a bitmap image using the BSP_LCD_DrawBitmap() function.

   + Options
     o Configure the LTDC reload mode by calling BSP_LCD_Relaod(). By default, the
       reload mode is set to BSP_LCD_RELOAD_IMMEDIATE then LTDC is reloaded immediately.
       To control the reload mode:
         - Call BSP_LCD_Relaod() with ReloadType parameter set to BSP_LCD_RELOAD_NONE
         - Configure LTDC (color keying, transparency ..)
         - Call BSP_LCD_Relaod() with ReloadType parameter set to BSP_LCD_RELOAD_IMMEDIATE
           for immediate reload or BSP_LCD_RELOAD_VERTICAL_BLANKING for LTDC reload
           in the next vertical blanking
     o Configure LTDC layers using BSP_LCD_ConfigLayer()
     o Control layer visibility using BSP_LCD_SetLayerVisible()
     o Configure and enable the color keying functionality using the
       BSP_LCD_SetColorKeying() function.
     o Disable the color keying functionality using the BSP_LCD_ResetColorKeying() function.
     o Modify on the fly the transparency and/or the frame buffer address
       using the following functions:
       - BSP_LCD_SetTransparency()
       - BSP_LCD_SetLayerAddress()

   + Display on LCD
     o To draw and fill a basic shapes (dot, line, rectangle, circle, ellipse, .. bitmap)
       on LCD and display text, utility basic_gui.c/.h must be called. Once the LCD is initialized,
       user should call GUI_SetFuncDriver() API to link board LCD drivers to BASIC GUI LCD drivers.
       The basic gui services, defined in basic_gui utility, are ready for use.

  Note:
  --------
    Regarding the "Instance" parameter, needed for all functions, it is used to select
    an LCD instance. On the STM32H747I_DISCO board, there's one instance. Then, this
    parameter should be 0.
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32h747i_discovery_lcd.h"
#include "stm32h747i_discovery_bus.h"
#include "stm32h747i_discovery_sdram.h"
/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32H747I_DISCO
  * @{
  */

/** @defgroup STM32H747I_DISCO_LCD LCD
  * @{
  */
/** @defgroup STM32H747I_DISCO_LCD_Private_Variables Private Variables
  * @{
  */
static LCD_Drv_t                *Lcd_Drv = NULL;
/**
  * @}
  */

/** @defgroup STM32H747I_DISCO_LCD_Private_TypesDefinitions Private TypesDefinitions
  * @{
  */
const GUI_Drv_t LCD_Driver =
{
  BSP_LCD_DrawBitmap,
  BSP_LCD_FillRGBRect,
  BSP_LCD_DrawHLine,
  BSP_LCD_DrawVLine,
  BSP_LCD_FillRect,
  BSP_LCD_ReadPixel,
  BSP_LCD_WritePixel,
  BSP_LCD_GetXSize,
  BSP_LCD_GetYSize,
  BSP_LCD_SetActiveLayer,
  BSP_LCD_GetPixelFormat
};

typedef struct
{
  uint32_t      HACT;
  uint32_t      VACT;
  uint32_t      HSYNC;
  uint32_t      HBP;
  uint32_t      HFP;
  uint32_t      VSYNC;
  uint32_t      VBP;
  uint32_t      VFP;

  /* Configure the D-PHY Timings */
  uint32_t      ClockLaneHS2LPTime;
  uint32_t      ClockLaneLP2HSTime;
  uint32_t      DataLaneHS2LPTime;
  uint32_t      DataLaneLP2HSTime;
  uint32_t      DataLaneMaxReadTime;
  uint32_t      StopWaitTime;
} LCD_HDMI_Timing_t;
/**
  * @}
  */

/** @defgroup STM32H747I_DISCO_LCD_Exported_Variables Exported Variables
  * @{
  */
void                *Lcd_CompObj = NULL;
DSI_HandleTypeDef   hlcd_dsi;
DMA2D_HandleTypeDef hlcd_dma2d;
LTDC_HandleTypeDef  hlcd_ltdc;
BSP_LCD_Ctx_t       Lcd_Ctx[LCD_INSTANCES_NBR];
/**
  * @}
  */

/** @defgroup STM32H747I_DISCO_LCD_Private_FunctionPrototypes Private FunctionPrototypes
  * @{
  */
static void DSI_MspInit(DSI_HandleTypeDef *hdsi);
static void DSI_MspDeInit(DSI_HandleTypeDef *hdsi);
static int32_t DSI_IO_Write(uint16_t ChannelNbr, uint16_t Reg, uint8_t *pData, uint16_t Size);
static int32_t DSI_IO_Read(uint16_t ChannelNbr, uint16_t Reg, uint8_t *pData, uint16_t Size);

#if (USE_LCD_CTRL_OTM8009A > 0)
static int32_t OTM8009A_Probe(uint32_t ColorCoding, uint32_t Orientation);
# endif /* USE_LCD_CTRL_OTM8009A > 0 */

#if (USE_LCD_CTRL_ADV7533 > 0)
static int32_t ADV7533_Probe(void);
static void LCD_Get_HDMI_VideoModeTiming(uint32_t Format, LCD_HDMI_Timing_t *Timing);
static void LTDC_HDMI_Init(LCD_HDMI_Timing_t *Timing);
#endif /* (USE_LCD_CTRL_ADV7533 > 0) */

static void LTDC_MspInit(LTDC_HandleTypeDef *hltdc);
static void LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc);
static void DMA2D_MspInit(DMA2D_HandleTypeDef *hdma2d);
static void DMA2D_MspDeInit(DMA2D_HandleTypeDef *hdma2d);
static void LL_FillBuffer(uint32_t Instance, uint32_t *pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLine, uint32_t Color);
static void LL_ConvertLineToRGB(uint32_t Instance, uint32_t *pSrc, uint32_t *pDst, uint32_t xSize, uint32_t ColorMode);
static void LCD_InitSequence(void);
static void LCD_DeInitSequence(void);
/**
  * @}
  */
/** @defgroup STM32H747I_DISCO_LCD_Private_Macros Private Macros
  * @{
  */
#define CONVERTRGB5652ARGB8888(Color)((((((((Color) >> (11U)) & 0x1FU) * 527U) + 23U) >> (6U)) << (16U)) |\
                                     (((((((Color) >> (5U)) & 0x3FU) * 259U) + 33U) >> (6U)) << (8U)) |\
                                     (((((Color) & 0x1FU) * 527U) + 23U) >> (6U)) | (0xFF000000U))

/**
  * @}
  */
/** @defgroup STM32H747I_DISCO_LCD_Exported_Functions LCD Exported Functions
  * @{
  */


#define LTDC_VSYNC               1 
#define LTDC_VBP                 1 
#define LTDC_VFP                 1
#define LTDC_VACT                480
#define LTDC_HSYNC               1
#define LTDC_HBP                 1
#define LTDC_HFP                 1
#define LTDC_HACT                800

void LTDC_CMD_Init(void)
{
  /* DeInit */
  HAL_LTDC_DeInit(&hlcd_ltdc);
  
  /* LTDC Config */
  /* Timing and polarity */
  hlcd_ltdc.Init.HorizontalSync = LTDC_HSYNC;
  hlcd_ltdc.Init.VerticalSync = LTDC_VSYNC;
  hlcd_ltdc.Init.AccumulatedHBP = LTDC_HSYNC+LTDC_HBP;
  hlcd_ltdc.Init.AccumulatedVBP = LTDC_VSYNC+LTDC_VBP;
  hlcd_ltdc.Init.AccumulatedActiveH = LTDC_VSYNC+LTDC_VBP+LTDC_VACT;
  hlcd_ltdc.Init.AccumulatedActiveW = LTDC_HSYNC+LTDC_HBP+LTDC_HACT;
  hlcd_ltdc.Init.TotalHeigh = LTDC_VSYNC+LTDC_VBP+LTDC_VACT+LTDC_VFP;
  hlcd_ltdc.Init.TotalWidth = LTDC_HSYNC+LTDC_HBP+LTDC_HACT+LTDC_HFP;
  
  /* background value */
  hlcd_ltdc.Init.Backcolor.Blue = 0;
  hlcd_ltdc.Init.Backcolor.Green = 0;
  hlcd_ltdc.Init.Backcolor.Red = 0;
  
  /* Polarity */
  hlcd_ltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hlcd_ltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hlcd_ltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hlcd_ltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hlcd_ltdc.Instance = LTDC;

  HAL_LTDC_Init(&hlcd_ltdc);
}

void LCD_CMD_MspInit(void)
{
  /** @brief Enable the LTDC clock */
  __HAL_RCC_LTDC_CLK_ENABLE();

  /** @brief Toggle Sw reset of LTDC IP */
  __HAL_RCC_LTDC_FORCE_RESET();
  __HAL_RCC_LTDC_RELEASE_RESET();

  /** @brief Enable the DMA2D clock */
  __HAL_RCC_DMA2D_CLK_ENABLE();

  /** @brief Toggle Sw reset of DMA2D IP */
  __HAL_RCC_DMA2D_FORCE_RESET();
  __HAL_RCC_DMA2D_RELEASE_RESET();

  /** @brief Enable DSI Host and wrapper clocks */
  __HAL_RCC_DSI_CLK_ENABLE();

  /** @brief Soft Reset the DSI Host and wrapper */
  __HAL_RCC_DSI_FORCE_RESET();
  __HAL_RCC_DSI_RELEASE_RESET();

  /** @brief NVIC configuration for LTDC interrupt that is now enabled */
  HAL_NVIC_SetPriority(LTDC_IRQn, 9, 0xf);
  HAL_NVIC_EnableIRQ(LTDC_IRQn);

  /** @brief NVIC configuration for DMA2D interrupt that is now enabled */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 9, 0xf);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);

  /** @brief NVIC configuration for DSI interrupt that is now enabled */
  HAL_NVIC_SetPriority(DSI_IRQn, 9, 0xf);
  HAL_NVIC_EnableIRQ(DSI_IRQn);
}

uint8_t BSP_LCD_CMD_Init(void)
{
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
  DSI_PLLInitTypeDef dsiPllInit;
  DSI_CmdCfgTypeDef CmdCfg;
  DSI_LPCmdTypeDef LPCmd;
  DSI_PHY_TimerTypeDef  PhyTimings;
  
  OTM8009A_IO_t       IOCtx;
  OTM8009A_Object_t   OTM8009AObj;

  /* Toggle Hardware Reset of the DSI LCD using
     its XRES signal (active low) */
  BSP_LCD_Reset(0);

  /* LCD clock configuration */
  /* LCD clock configuration */
  /* PLL3_VCO Input = HSE_VALUE/PLL3M = 5 Mhz */
  /* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 800 Mhz */
  /* PLLLCDCLK = PLL3_VCO Output/PLL3R = 800/19 = 42 Mhz */
  /* LTDC clock frequency = PLLLCDCLK = 42 Mhz */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M = 5;    
  PeriphClkInitStruct.PLL3.PLL3N = 160;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;  
  PeriphClkInitStruct.PLL3.PLL3R = 19;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

  /* Call first MSP Initialize only in case of first initialization
  * This will set IP blocks LTDC, DSI and DMA2D
  * - out of reset
  * - clocked
  * - NVIC IRQ related to IP blocks enabled
  */
  LCD_CMD_MspInit();

  /* Base address of DSI Host/Wrapper registers to be set before calling De-Init */
  hlcd_dsi.Instance = DSI;

  HAL_DSI_DeInit(&(hlcd_dsi));

  dsiPllInit.PLLNDIV  = 100;
  dsiPllInit.PLLIDF   = DSI_PLL_IN_DIV5;
  dsiPllInit.PLLODF  = DSI_PLL_OUT_DIV1;  

  hlcd_dsi.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
  hlcd_dsi.Init.TXEscapeCkdiv = 0x4;

  HAL_DSI_Init(&(hlcd_dsi), &(dsiPllInit));

  /* Configure the DSI for Command mode */
  CmdCfg.VirtualChannelID      = 0;
  CmdCfg.HSPolarity            = DSI_HSYNC_ACTIVE_HIGH;
  CmdCfg.VSPolarity            = DSI_VSYNC_ACTIVE_HIGH;
  CmdCfg.DEPolarity            = DSI_DATA_ENABLE_ACTIVE_HIGH;
  CmdCfg.ColorCoding           = DSI_RGB888;
  CmdCfg.CommandSize           = LTDC_HACT;
  CmdCfg.TearingEffectSource   = DSI_TE_DSILINK;
  CmdCfg.TearingEffectPolarity = DSI_TE_RISING_EDGE;
  CmdCfg.VSyncPol              = DSI_VSYNC_FALLING;
  CmdCfg.AutomaticRefresh      = DSI_AR_DISABLE;
  CmdCfg.TEAcknowledgeRequest  = DSI_TE_ACKNOWLEDGE_ENABLE;
  HAL_DSI_ConfigAdaptedCommandMode(&hlcd_dsi, &CmdCfg);

  LPCmd.LPGenShortWriteNoP    = DSI_LP_GSW0P_ENABLE;
  LPCmd.LPGenShortWriteOneP   = DSI_LP_GSW1P_ENABLE;
  LPCmd.LPGenShortWriteTwoP   = DSI_LP_GSW2P_ENABLE;
  LPCmd.LPGenShortReadNoP     = DSI_LP_GSR0P_ENABLE;
  LPCmd.LPGenShortReadOneP    = DSI_LP_GSR1P_ENABLE;
  LPCmd.LPGenShortReadTwoP    = DSI_LP_GSR2P_ENABLE;
  LPCmd.LPGenLongWrite        = DSI_LP_GLW_ENABLE;
  LPCmd.LPDcsShortWriteNoP    = DSI_LP_DSW0P_ENABLE;
  LPCmd.LPDcsShortWriteOneP   = DSI_LP_DSW1P_ENABLE;
  LPCmd.LPDcsShortReadNoP     = DSI_LP_DSR0P_ENABLE;
  LPCmd.LPDcsLongWrite        = DSI_LP_DLW_ENABLE;
  HAL_DSI_ConfigCommand(&hlcd_dsi, &LPCmd);

  /* Initialize LTDC */
  LTDC_CMD_Init();

  /* Start DSI */
  HAL_DSI_Start(&(hlcd_dsi));

  /* Configure DSI PHY HS2LP and LP2HS timings */
  PhyTimings.ClockLaneHS2LPTime = 35;
  PhyTimings.ClockLaneLP2HSTime = 35;
  PhyTimings.DataLaneHS2LPTime = 35;
  PhyTimings.DataLaneLP2HSTime = 35;
  PhyTimings.DataLaneMaxReadTime = 0;
  PhyTimings.StopWaitTime = 10;
  HAL_DSI_ConfigPhyTimer(&hlcd_dsi, &PhyTimings);   

  OTM8009A_Probe(OTM8009A_COLMOD_RGB888, LCD_ORIENTATION_LANDSCAPE);

  LPCmd.LPGenShortWriteNoP    = DSI_LP_GSW0P_DISABLE;
  LPCmd.LPGenShortWriteOneP   = DSI_LP_GSW1P_DISABLE;
  LPCmd.LPGenShortWriteTwoP   = DSI_LP_GSW2P_DISABLE;
  LPCmd.LPGenShortReadNoP     = DSI_LP_GSR0P_DISABLE;
  LPCmd.LPGenShortReadOneP    = DSI_LP_GSR1P_DISABLE;
  LPCmd.LPGenShortReadTwoP    = DSI_LP_GSR2P_DISABLE;
  LPCmd.LPGenLongWrite        = DSI_LP_GLW_DISABLE;
  LPCmd.LPDcsShortWriteNoP    = DSI_LP_DSW0P_DISABLE;
  LPCmd.LPDcsShortWriteOneP   = DSI_LP_DSW1P_DISABLE;
  LPCmd.LPDcsShortReadNoP     = DSI_LP_DSR0P_DISABLE;
  LPCmd.LPDcsLongWrite        = DSI_LP_DLW_DISABLE;
  HAL_DSI_ConfigCommand(&hlcd_dsi, &LPCmd);
  
  HAL_DSI_ConfigFlowControl(&hlcd_dsi, DSI_FLOW_CONTROL_BTA);
  HAL_DSI_ForceRXLowPower(&hlcd_dsi, ENABLE);  

  return BSP_ERROR_NONE;
}


/**
  * @brief  Initializes the LCD in default mode.
  * @param  Instance    LCD Instance
  * @param  Orientation LCD_ORIENTATION_PORTRAIT or LCD_ORIENTATION_LANDSCAPE
  * @retval BSP status
  */
int32_t BSP_LCD_Init(uint32_t Instance, uint32_t Orientation)
{
  return BSP_LCD_InitEx(Instance, Orientation, LCD_PIXEL_FORMAT_RGB888, LCD_DEFAULT_WIDTH, LCD_DEFAULT_HEIGHT);
}

int BSP_LCD_UseHDMI (void)
{
    return 0;
}

/**
  * @brief  Initializes the LCD.
  * @param  Instance    LCD Instance
  * @param  Orientation LCD_ORIENTATION_PORTRAIT or LCD_ORIENTATION_LANDSCAPE
  * @param  PixelFormat LCD_PIXEL_FORMAT_RBG565 or LCD_PIXEL_FORMAT_RBG888
  * @param  Width       Display width
  * @param  Height      Display height
  * @retval BSP status
  */
int32_t BSP_LCD_InitEx(uint32_t Instance, uint32_t Orientation, uint32_t PixelFormat, uint32_t Width, uint32_t Height)
{
  int32_t ret = BSP_ERROR_NONE;
  uint32_t ctrl_pixel_format, dsi_pixel_format;

  if((Orientation > LCD_ORIENTATION_LANDSCAPE) || (Instance >= LCD_INSTANCES_NBR) || \
     ((PixelFormat != LCD_PIXEL_FORMAT_RGB565) && (PixelFormat != LTDC_PIXEL_FORMAT_RGB888)))
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(PixelFormat == LCD_PIXEL_FORMAT_RGB565)
    {
      dsi_pixel_format = DSI_RGB565;
      ctrl_pixel_format = OTM8009A_FORMAT_RBG565;
      Lcd_Ctx[Instance].BppFactor = 2U;
    }
    else /* LCD_PIXEL_FORMAT_RGB888 */
    {
      dsi_pixel_format = DSI_RGB888;
      ctrl_pixel_format = OTM8009A_FORMAT_RGB888;
      Lcd_Ctx[Instance].BppFactor = 4U;
    }

    /* Store pixel format, xsize and ysize information */
    Lcd_Ctx[Instance].PixelFormat = PixelFormat;
    Lcd_Ctx[Instance].XSize  = Width;
    Lcd_Ctx[Instance].YSize  = Height;

    /* Toggle Hardware Reset of the LCD using its XRES signal (active low) */
    BSP_LCD_Reset(Instance);


    /* Initialize LCD special pins GPIOs */
    LCD_InitSequence();

    /* Initializes peripherals instance value */
    hlcd_ltdc.Instance = LTDC;
    hlcd_dma2d.Instance = DMA2D;
    hlcd_dsi.Instance = DSI;

    /* MSP initialization */
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
    /* Register the LTDC MSP Callbacks */
    if(Lcd_Ctx[Instance].IsMspCallbacksValid == 0U)
    {
      if(BSP_LCD_RegisterDefaultMspCallbacks(0) != BSP_ERROR_NONE)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }
    }
#else
    LTDC_MspInit(&hlcd_ltdc);
#endif

    DMA2D_MspInit(&hlcd_dma2d);

#if (USE_HAL_DSI_REGISTER_CALLBACKS == 1)
    /* Register the DSI MSP Callbacks */
    if(Lcd_Ctx[Instance].IsMspCallbacksValid == 0U)
    {
      if(BSP_LCD_RegisterDefaultMspCallbacks(0) != BSP_ERROR_NONE)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }
    }
#else
    DSI_MspInit(&hlcd_dsi);
#endif
    if(MX_DSIHOST_DSI_Init(&hlcd_dsi, Width, Height, dsi_pixel_format) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else if(MX_LTDC_ClockConfig(&hlcd_ltdc) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
     if(MX_LTDC_Init(&hlcd_ltdc, Width, Height) != HAL_OK)
     {
       ret = BSP_ERROR_PERIPH_FAILURE;
     }
    }

    if(ret == BSP_ERROR_NONE)
    {
      /* Before configuring LTDC layer, ensure SDRAM is initialized */
#if !defined(DATA_IN_ExtSDRAM)
      /* Initialize the SDRAM */
      if(BSP_SDRAM_Init(0) != BSP_ERROR_NONE)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }
#endif /* DATA_IN_ExtSDRAM */
      {
        /* Enable the DSI host and wrapper after the LTDC initialization
        To avoid any synchronization issue, the DSI shall be started after enabling the LTDC */
        (void)HAL_DSI_Start(&hlcd_dsi);

        /* Enable the DSI BTW for read operations */
        (void)HAL_DSI_ConfigFlowControl(&hlcd_dsi, DSI_FLOW_CONTROL_BTA);

#if (USE_LCD_CTRL_OTM8009A == 1)
        /* Initialize the OTM8009A LCD Display IC Driver (KoD LCD IC Driver)
        depending on configuration of DSI */
        if(OTM8009A_Probe(ctrl_pixel_format, Orientation) != BSP_ERROR_NONE)
        {
          ret = BSP_ERROR_UNKNOWN_COMPONENT;
        }
        else
        {
          ret = BSP_ERROR_NONE;
        }
#endif
      }
    /* By default the reload is activated and executed immediately */
    Lcd_Ctx[Instance].ReloadEnable = 1U;
   }
  }
  return ret;
}

/**
  * @brief  De-Initializes the LCD resources.
  * @param  Instance    LCD Instance
  * @retval BSP status
  */
int32_t BSP_LCD_DeInit(uint32_t Instance)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    LCD_DeInitSequence();
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 0)
    LTDC_MspDeInit(&hlcd_ltdc);
#endif /* (USE_HAL_LTDC_REGISTER_CALLBACKS == 0) */

    DMA2D_MspDeInit(&hlcd_dma2d);

#if (USE_HAL_DSI_REGISTER_CALLBACKS == 0)
    DSI_MspDeInit(&hlcd_dsi);
#endif /* (USE_HAL_DSI_REGISTER_CALLBACKS == 0) */
    if(HAL_DSI_DeInit(&hlcd_dsi) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
      (void)HAL_LTDC_DeInit(&hlcd_ltdc);
      if(HAL_DMA2D_DeInit(&hlcd_dma2d) != HAL_OK)
      {
        ret = BSP_ERROR_PERIPH_FAILURE;
      }
      else
      {
        Lcd_Ctx[Instance].IsMspCallbacksValid = 0;
      }
    }
  }

  return ret;
}


void BSP_LCD_DeInitEx (void)
{
    BSP_LCD_DeInit(0);
}

#if (USE_LCD_CTRL_ADV7533 > 0)
/**
  * @brief  Initializes the LCD HDMI Mode.
  * @param  Instance    LCD Instance
  * @param  Format      HDMI format could be HDMI_FORMAT_720_480 or HDMI_FORMAT_720_576
  * @retval BSP status
  */
int32_t BSP_LCD_InitHDMI(uint32_t Instance, uint32_t Format)
{
  int32_t ret;
  DSI_PLLInitTypeDef    dsiPllInit;
  DSI_PHY_TimerTypeDef  dsiPhyInit;
  LCD_HDMI_Timing_t     hdmi_timing;
  DSI_VidCfgTypeDef     hDsiVideoMode;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    LCD_Get_HDMI_VideoModeTiming(Format, &hdmi_timing);
    Lcd_Ctx[Instance].XSize       = hdmi_timing.HACT;
    Lcd_Ctx[Instance].YSize       = hdmi_timing.VACT;
    Lcd_Ctx[Instance].PixelFormat = LCD_PIXEL_FORMAT_RGB888;

    /* Toggle Hardware Reset of the DSI LCD using
    * its XRES signal (active low) */
    BSP_LCD_Reset(Instance);

    if(ADV7533_Probe() != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
    else if(Lcd_Drv->DisplayOn(Lcd_CompObj)!= BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
    else
    {
      /* Call first MSP Initialize only in case of first initialization
      * This will set IP blocks LTDC, DSI and DMA2D
      * - out of reset
      * - clocked
      * - NVIC IRQ related to IP blocks enabled
      */
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
      /* Register the LTDC MSP Callbacks */
      if(Lcd_Ctx[Instance].IsMspCallbacksValid == 0U)
      {
        if(BSP_LCD_RegisterDefaultMspCallbacks(0) != BSP_ERROR_NONE)
        {
          return BSP_ERROR_PERIPH_FAILURE;
        }
      }
#else
      LTDC_MspInit(&hlcd_ltdc);
#endif

      DMA2D_MspInit(&hlcd_dma2d);

#if (USE_HAL_DSI_REGISTER_CALLBACKS == 1)
      /* Register the DSI MSP Callbacks */
      if(Lcd_Ctx[Instance].IsMspCallbacksValid == 0U)
      {
        if(BSP_LCD_RegisterDefaultMspCallbacks(0) != BSP_ERROR_NONE)
        {
          return BSP_ERROR_PERIPH_FAILURE;
        }
      }
#else
      DSI_MspInit(&hlcd_dsi);
#endif

      /*************************DSI Initialization***********************************/
      /* Base address of DSI Host/Wrapper registers to be set before calling De-Init */
      hlcd_dsi.Instance = DSI;
      /* Set number of Lanes */
      hlcd_dsi.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
      /* Set the TX escape clock division ratio */
      hlcd_dsi.Init.TXEscapeCkdiv = 3U;

      /* Configure the DSI PLL */
      dsiPllInit.PLLNDIV    = 65U;
      dsiPllInit.PLLIDF     = DSI_PLL_IN_DIV5;
      dsiPllInit.PLLODF     = DSI_PLL_OUT_DIV1;
      if(HAL_DSI_Init(&hlcd_dsi, &dsiPllInit) != HAL_OK)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }

      /* Configure the D-PHY Timings */
      dsiPhyInit.ClockLaneHS2LPTime  = hdmi_timing.ClockLaneHS2LPTime;
      dsiPhyInit.ClockLaneLP2HSTime  = hdmi_timing.ClockLaneLP2HSTime;
      dsiPhyInit.DataLaneHS2LPTime   = hdmi_timing.DataLaneHS2LPTime;
      dsiPhyInit.DataLaneLP2HSTime   = hdmi_timing.DataLaneLP2HSTime;
      dsiPhyInit.DataLaneMaxReadTime = hdmi_timing.DataLaneMaxReadTime;
      dsiPhyInit.StopWaitTime        = hdmi_timing.StopWaitTime;
      if(HAL_DSI_ConfigPhyTimer(&hlcd_dsi, &dsiPhyInit) != HAL_OK)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }

      /* Timing parameters for all Video modes */
      /*
      The lane byte clock is set 40625 Khz
      The pixel clock is set to 27083 Khz
      */
      hDsiVideoMode.VirtualChannelID = 0;
      hDsiVideoMode.ColorCoding      = DSI_RGB888;
      hDsiVideoMode.LooselyPacked    = DSI_LOOSELY_PACKED_DISABLE;
      hDsiVideoMode.VSPolarity       = DSI_VSYNC_ACTIVE_LOW;
      hDsiVideoMode.HSPolarity       = DSI_HSYNC_ACTIVE_LOW;
      hDsiVideoMode.DEPolarity       = DSI_DATA_ENABLE_ACTIVE_HIGH;
      hDsiVideoMode.Mode             = DSI_VID_MODE_NB_PULSES;
      hDsiVideoMode.NullPacketSize   = 0U;
      hDsiVideoMode.NumberOfChunks   = 1U;
      hDsiVideoMode.PacketSize       = hdmi_timing.HACT; /* Value depending on display format */
      hDsiVideoMode.HorizontalSyncActive = (hdmi_timing.HSYNC * 40625U)/27083U;
      hDsiVideoMode.HorizontalBackPorch  = (hdmi_timing.HBP * 40625U)/27083U;
      hDsiVideoMode.HorizontalLine = ((hdmi_timing.HACT + hdmi_timing.HSYNC + hdmi_timing.HBP + hdmi_timing.HFP) * 40625U)/27083U; /* Value depending on display format */
      hDsiVideoMode.VerticalSyncActive        = hdmi_timing.VSYNC;
      hDsiVideoMode.VerticalBackPorch         = hdmi_timing.VBP;
      hDsiVideoMode.VerticalFrontPorch        = hdmi_timing.VFP;
      hDsiVideoMode.VerticalActive            = hdmi_timing.VACT; /* Value depending on display format */

      /* Enable or disable sending LP command while streaming is active in video mode */
      hDsiVideoMode.LPCommandEnable = DSI_LP_COMMAND_ENABLE; /* Enable sending commands in mode LP (Low Power) */

      /* Largest packet size possible to transmit in LP mode in VSA, VBP, VFP regions */
      /* Only useful when sending LP packets is allowed while streaming is active in video mode */
      hDsiVideoMode.LPLargestPacketSize     = 4;

      /* Largest packet size possible to transmit in LP mode in HFP region during VACT period */
      /* Only useful when sending LP packets is allowed while streaming is active in video mode */
      hDsiVideoMode.LPVACTLargestPacketSize = 4;

      /* Specify for each region of the video frame, if the transmission of command in LP mode is allowed in this region */
      /* while streaming is active in video mode                                                                         */
      hDsiVideoMode.LPHorizontalFrontPorchEnable = DSI_LP_HFP_ENABLE;   /* Allow sending LP commands during HFP period */
      hDsiVideoMode.LPHorizontalBackPorchEnable  = DSI_LP_HBP_ENABLE;   /* Allow sending LP commands during HBP period */
      hDsiVideoMode.LPVerticalActiveEnable       = DSI_LP_VACT_ENABLE;  /* Allow sending LP commands during VACT period */
      hDsiVideoMode.LPVerticalFrontPorchEnable   = DSI_LP_VFP_ENABLE;   /* Allow sending LP commands during VFP period */
      hDsiVideoMode.LPVerticalBackPorchEnable    = DSI_LP_VBP_ENABLE;   /* Allow sending LP commands during VBP period */
      hDsiVideoMode.LPVerticalSyncActiveEnable   = DSI_LP_VSYNC_ENABLE; /* Allow sending LP commands during VSync = VSA period */

      /* Configure DSI Video mode timings with settings set above */
      (void)HAL_DSI_ConfigVideoMode(&(hlcd_dsi), &(hDsiVideoMode));

      if(MX_LTDC_ClockConfig2(&hlcd_ltdc) != HAL_OK)
      {
        return BSP_ERROR_CLOCK_FAILURE;
      }

      /* Initialize LTDC */

      /* Enable the DSI host and wrapper after the LTDC initialization
      To avoid any synchronization issue, the DSI shall be started after enabling the LTDC */
      (void)HAL_DSI_Start(&(hlcd_dsi));

#if !defined(DATA_IN_ExtSDRAM)
      /* Initialize the SDRAM */
      if(BSP_SDRAM_Init(0) != BSP_ERROR_NONE)
      {
        return BSP_ERROR_PERIPH_FAILURE;
      }
#endif /* DATA_IN_ExtSDRAM */

      ret = BSP_ERROR_NONE;
    }
  }

  return ret;
}

#endif /*USE_LCD_CTRL_ADV7533 > 0*/
/**
  * @brief  BSP LCD Reset
  *         Hw reset the LCD DSI activating its XRES signal (active low for some time)
  *         and deactivating it later.
  * @param  Instance LCD Instance
  */
void BSP_LCD_Reset(uint32_t Instance)
{
  GPIO_InitTypeDef  gpio_init_structure;

  LCD_RESET_GPIO_CLK_ENABLE();

  /* Configure the GPIO Reset pin */
  gpio_init_structure.Pin   = LCD_RESET_PIN;
  gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Pull  = GPIO_PULLUP;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_PORT , &gpio_init_structure);

  /* Activate XRES active low */
  HAL_GPIO_WritePin(LCD_RESET_GPIO_PORT , LCD_RESET_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);/* wait 20 ms */
  HAL_GPIO_WritePin(LCD_RESET_GPIO_PORT , LCD_RESET_PIN, GPIO_PIN_SET);/* Deactivate XRES */
  HAL_Delay(10);/* Wait for 10ms after releasing XRES before sending commands */
}

/**
  * @brief  Configure LCD control pins (Back-light, Display Enable and TE)
  * @retval None
  */
static void LCD_InitSequence(void)
{
  GPIO_InitTypeDef  gpio_init_structure;

  /* LCD_BL_CTRL GPIO configuration */
  LCD_BL_CTRL_GPIO_CLK_ENABLE();

  gpio_init_structure.Pin       = LCD_BL_CTRL_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(LCD_BL_CTRL_GPIO_PORT, &gpio_init_structure);
  /* Assert back-light LCD_BL_CTRL pin */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);

  /* LCD_TE_CTRL GPIO configuration */
  LCD_TE_GPIO_CLK_ENABLE();

  gpio_init_structure.Pin       = LCD_TE_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;

  HAL_GPIO_Init(LCD_TE_GPIO_PORT, &gpio_init_structure);
  /* Assert back-light LCD_BL_CTRL pin */
  HAL_GPIO_WritePin(LCD_TE_GPIO_PORT, LCD_TE_PIN, GPIO_PIN_SET);

      /** @brief NVIC configuration for LTDC interrupt that is now enabled */
  HAL_NVIC_SetPriority(LTDC_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(LTDC_IRQn);

  /** @brief NVIC configuration for DMA2D interrupt that is now enabled */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);

  /** @brief NVIC configuration for DSI interrupt that is now enabled */
  HAL_NVIC_SetPriority(DSI_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(DSI_IRQn);
}

/**
  * @brief  DeInitializes LCD GPIO special pins MSP.
  * @retval None
  */
static void LCD_DeInitSequence(void)
{
  GPIO_InitTypeDef  gpio_init_structure;

  /* LCD_BL_CTRL GPIO configuration */
  /* LCD_BL_CTRL GPIO deactivation */
  gpio_init_structure.Pin       = LCD_BL_CTRL_PIN;
  HAL_GPIO_DeInit(LCD_BL_CTRL_GPIO_PORT, gpio_init_structure.Pin);

  /* LCD_TE_CTRL GPIO configuration */
}

/**
  * @brief  Initializes the DSIHOST.
  * @param  hdsi   DSI handle
  * @param  Width  Horizontal active width
  * @param  Height Vertical active height
  * @param  PixelFormat DSI color coding RGB888 or RGB565
  * @retval HAL status
  */
__weak HAL_StatusTypeDef MX_DSIHOST_DSI_Init(DSI_HandleTypeDef *hdsi, uint32_t Width, uint32_t Height, uint32_t PixelFormat)
{
  DSI_PLLInitTypeDef PLLInit = {0};
  DSI_VidCfgTypeDef VidCfg = {0};

  hdsi->Instance = DSI;
  hdsi->Init.AutomaticClockLaneControl = DSI_AUTO_CLK_LANE_CTRL_DISABLE;
  hdsi->Init.TXEscapeCkdiv = 4;
  hdsi->Init.NumberOfLanes = DSI_TWO_DATA_LANES;
  PLLInit.PLLNDIV = 100;
  PLLInit.PLLIDF = DSI_PLL_IN_DIV5;
  PLLInit.PLLODF = DSI_PLL_OUT_DIV1;
  if (HAL_DSI_Init(hdsi, &PLLInit) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* Timing parameters for all Video modes */
  /*
  The lane byte clock is set 62500 Khz
  The pixel clock is set to 27429 Khz
  */
  VidCfg.VirtualChannelID = 0;
  VidCfg.ColorCoding = PixelFormat;
  VidCfg.LooselyPacked = DSI_LOOSELY_PACKED_DISABLE;
  VidCfg.Mode = DSI_VID_MODE_BURST;
  VidCfg.PacketSize = Width;
  VidCfg.NumberOfChunks = 0;
  VidCfg.NullPacketSize = 0xFFFU;
  VidCfg.HSPolarity = DSI_HSYNC_ACTIVE_HIGH;
  VidCfg.VSPolarity = DSI_VSYNC_ACTIVE_HIGH;
  VidCfg.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;
  VidCfg.HorizontalSyncActive = (OTM8009A_480X800_HSYNC * 62500U)/27429U;
  VidCfg.HorizontalBackPorch = (OTM8009A_480X800_HBP * 62500U)/27429U;
  VidCfg.HorizontalLine = ((Width + OTM8009A_480X800_HSYNC + OTM8009A_480X800_HBP + OTM8009A_480X800_HFP) * 62500U)/27429U;
  VidCfg.VerticalSyncActive = OTM8009A_480X800_VSYNC;
  VidCfg.VerticalBackPorch = OTM8009A_480X800_VBP;
  VidCfg.VerticalFrontPorch = OTM8009A_480X800_VFP;
  VidCfg.VerticalActive = Height;
  VidCfg.LPCommandEnable = DSI_LP_COMMAND_ENABLE;
  VidCfg.LPLargestPacketSize = 4;
  VidCfg.LPVACTLargestPacketSize = 4;

  VidCfg.LPHorizontalFrontPorchEnable  = DSI_LP_HFP_ENABLE;
  VidCfg.LPHorizontalBackPorchEnable   = DSI_LP_HBP_ENABLE;
  VidCfg.LPVerticalActiveEnable        = DSI_LP_VACT_ENABLE;
  VidCfg.LPVerticalFrontPorchEnable    = DSI_LP_VFP_ENABLE;
  VidCfg.LPVerticalBackPorchEnable     = DSI_LP_VBP_ENABLE;
  VidCfg.LPVerticalSyncActiveEnable    = DSI_LP_VSYNC_ENABLE;

  if (HAL_DSI_ConfigVideoMode(hdsi, &VidCfg) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
  * @brief  Initializes the LTDC.
  * @param  hltdc  LTDC handle
  * @param  Width  LTDC width
  * @param  Height LTDC height
  * @retval HAL status
  */
__weak HAL_StatusTypeDef MX_LTDC_Init(LTDC_HandleTypeDef *hltdc, uint32_t Width, uint32_t Height)
{
  hltdc->Instance = LTDC;
  hltdc->Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc->Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc->Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc->Init.PCPolarity = LTDC_PCPOLARITY_IPC;

  hltdc->Init.HorizontalSync     = OTM8009A_480X800_HSYNC - 1;
  hltdc->Init.AccumulatedHBP     = OTM8009A_480X800_HSYNC + OTM8009A_480X800_HBP - 1;
  hltdc->Init.AccumulatedActiveW = OTM8009A_480X800_HSYNC + Width + OTM8009A_480X800_HBP - 1;
  hltdc->Init.TotalWidth         = OTM8009A_480X800_HSYNC + Width + OTM8009A_480X800_HBP + OTM8009A_480X800_HFP - 1;
  hltdc->Init.VerticalSync       = OTM8009A_480X800_VSYNC - 1;
  hltdc->Init.AccumulatedVBP     = OTM8009A_480X800_VSYNC + OTM8009A_480X800_VBP - 1;
  hltdc->Init.AccumulatedActiveH = OTM8009A_480X800_VSYNC + Height + OTM8009A_480X800_VBP - 1;
  hltdc->Init.TotalHeigh         = OTM8009A_480X800_VSYNC + Height + OTM8009A_480X800_VBP + OTM8009A_480X800_VFP - 1;

  hltdc->Init.Backcolor.Blue  = 0x00;
  hltdc->Init.Backcolor.Green = 0x00;
  hltdc->Init.Backcolor.Red   = 0x00;

  return HAL_LTDC_Init(hltdc);
}

/**
  * @brief  MX LTDC layer configuration.
  * @param  hltdc      LTDC handle
  * @param  LayerIndex Layer 0 or 1
  * @param  Config     Layer configuration
  * @retval HAL status
  */
__weak HAL_StatusTypeDef MX_LTDC_ConfigLayer(LTDC_HandleTypeDef *hltdc, uint32_t LayerIndex, MX_LTDC_LayerConfig_t *Config)
{
  LTDC_LayerCfgTypeDef pLayerCfg;

  pLayerCfg.WindowX0 = Config->X0;
  pLayerCfg.WindowX1 = Config->X1;
  pLayerCfg.WindowY0 = Config->Y0;
  pLayerCfg.WindowY1 = Config->Y1;
  pLayerCfg.PixelFormat = Config->PixelFormat;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg.FBStartAdress = Config->Address;
  pLayerCfg.ImageWidth = (Config->X1 - Config->X0);
  pLayerCfg.ImageHeight = (Config->Y1 - Config->Y0);
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  return HAL_LTDC_ConfigLayer(hltdc, &pLayerCfg, LayerIndex);
}

/**
  * @brief  LTDC Clock Config for LCD DSI display.
  * @param  hltdc  LTDC Handle
  *         Being __weak it can be overwritten by the application
  * @retval HAL_status
  */
__weak HAL_StatusTypeDef MX_LTDC_ClockConfig(LTDC_HandleTypeDef *hltdc)
{
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

  PeriphClkInitStruct.PeriphClockSelection   = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M      = 5U;
  PeriphClkInitStruct.PLL3.PLL3N      = 132U;
  PeriphClkInitStruct.PLL3.PLL3P      = 2U;
  PeriphClkInitStruct.PLL3.PLL3Q      = 2U;
  PeriphClkInitStruct.PLL3.PLL3R      = 24U;
  return HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}

/**
  * @brief  LTDC Clock Config for LCD 2 DPI display.
  * @param  hltdc  LTDC Handle
  *         Being __weak it can be overwritten by the application
  * @retval HAL_status
  */
__weak HAL_StatusTypeDef MX_LTDC_ClockConfig2(LTDC_HandleTypeDef *hltdc)
{
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

  PeriphClkInitStruct.PeriphClockSelection    = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLL3.PLL3M      = 1U;
  PeriphClkInitStruct.PLL3.PLL3N      = 720U;
  PeriphClkInitStruct.PLL3.PLL3P      = 2U;
  PeriphClkInitStruct.PLL3.PLL3Q      = 2U;
  PeriphClkInitStruct.PLL3.PLL3R      = 64U;
  return HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}

#if ((USE_HAL_LTDC_REGISTER_CALLBACKS == 1) || (USE_HAL_DSI_REGISTER_CALLBACKS == 1))
/**
  * @brief Default BSP LCD Msp Callbacks
  * @param Instance BSP LCD Instance
  * @retval BSP status
  */
int32_t BSP_LCD_RegisterDefaultMspCallbacks (uint32_t Instance)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
    if(HAL_LTDC_RegisterCallback(&hlcd_ltdc, HAL_LTDC_MSPINIT_CB_ID, LTDC_MspInit) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
      if(HAL_LTDC_RegisterCallback(&hlcd_ltdc, HAL_LTDC_MSPDEINIT_CB_ID, LTDC_MspDeInit) != HAL_OK)
      {
        ret = BSP_ERROR_PERIPH_FAILURE;
      }
    }
#endif /* (USE_HAL_LTDC_REGISTER_CALLBACKS == 1) */

#if (USE_HAL_DSI_REGISTER_CALLBACKS == 1)
    if(ret == BSP_ERROR_NONE)
    {
      if(HAL_DSI_RegisterCallback(&hlcd_dsi, HAL_DSI_MSPINIT_CB_ID, DSI_MspInit) != HAL_OK)
      {
        ret = BSP_ERROR_PERIPH_FAILURE;
      }
      else
      {
        if(HAL_DSI_RegisterCallback(&hlcd_dsi, HAL_DSI_MSPDEINIT_CB_ID, DSI_MspDeInit) != HAL_OK)
        {
          ret = BSP_ERROR_PERIPH_FAILURE;
        }
      }
    }
#endif /* (USE_HAL_DSI_REGISTER_CALLBACKS == 1) */

    Lcd_Ctx[Instance].IsMspCallbacksValid = 1;
  }

  return ret;
}

/**
  * @brief BSP LCD Msp Callback registering
  * @param Instance    LCD Instance
  * @param CallBacks   pointer to LCD MspInit/MspDeInit functions
  * @retval BSP status
  */
int32_t BSP_LCD_RegisterMspCallbacks (uint32_t Instance, BSP_LCD_Cb_t *CallBacks)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
    if(HAL_LTDC_RegisterCallback(&hlcd_ltdc, HAL_LTDC_MSPINIT_CB_ID, CallBacks->pMspLtdcInitCb) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
      if(HAL_LTDC_RegisterCallback(&hlcd_ltdc, HAL_LTDC_MSPDEINIT_CB_ID, CallBacks->pMspLtdcDeInitCb) != HAL_OK)
      {
        ret = BSP_ERROR_PERIPH_FAILURE;
      }
    }
#endif /* (USE_HAL_LTDC_REGISTER_CALLBACKS == 1) */

#if (USE_HAL_DSI_REGISTER_CALLBACKS == 1)
    if(ret == BSP_ERROR_NONE)
    {
      if(HAL_DSI_RegisterCallback(&hlcd_dsi, HAL_DSI_MSPINIT_CB_ID, CallBacks->pMspDsiInitCb) != HAL_OK)
      {
        ret = BSP_ERROR_PERIPH_FAILURE;
      }
      else
      {
        if(HAL_DSI_RegisterCallback(&hlcd_dsi, HAL_DSI_MSPDEINIT_CB_ID, CallBacks->pMspDsiDeInitCb) != HAL_OK)
        {
          ret = BSP_ERROR_PERIPH_FAILURE;
        }
      }
    }
#endif /* (USE_HAL_DSI_REGISTER_CALLBACKS == 1) */

    Lcd_Ctx[Instance].IsMspCallbacksValid = 1;
  }

  return ret;
}
#endif /*((USE_HAL_LTDC_REGISTER_CALLBACKS == 1) || (USE_HAL_DSI_REGISTER_CALLBACKS == 1)) */

/**
  * @brief  LTDC layer configuration.
  * @param  Instance   LCD instance
  * @param  LayerIndex Layer 0 or 1
  * @param  Config     Layer configuration
  * @retval HAL status
  */
int32_t BSP_LCD_ConfigLayer(uint32_t Instance, uint32_t LayerIndex, BSP_LCD_LayerConfig_t *Config)
{
  int32_t ret = BSP_ERROR_NONE;
  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if (MX_LTDC_ConfigLayer(&hlcd_ltdc, LayerIndex, Config) != HAL_OK)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Set the LCD Active Layer.
  * @param  Instance    LCD Instance
  * @param  LayerIndex  LCD layer index
  * @retval BSP status
  */
int32_t BSP_LCD_SetActiveLayer(uint32_t Instance, uint32_t LayerIndex)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    Lcd_Ctx[Instance].ActiveLayer = LayerIndex;
  }

  return ret;
}
/**
  * @brief  Gets the LCD Active LCD Pixel Format.
  * @param  Instance    LCD Instance
  * @param  PixelFormat Active LCD Pixel Format
  * @retval BSP status
  */
int32_t BSP_LCD_GetPixelFormat(uint32_t Instance, uint32_t *PixelFormat)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    /* Only RGB565 format is supported */
    *PixelFormat = Lcd_Ctx[Instance].PixelFormat;
  }

  return ret;
}
/**
  * @brief  Control the LTDC reload
  * @param  Instance    LCD Instance
  * @param  ReloadType can be one of the following values
  *         - BSP_LCD_RELOAD_NONE
  *         - BSP_LCD_RELOAD_IMMEDIATE
  *         - BSP_LCD_RELOAD_VERTICAL_BLANKING
  * @retval BSP status
  */
int32_t BSP_LCD_Relaod(uint32_t Instance, uint32_t ReloadType)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else if(ReloadType == BSP_LCD_RELOAD_NONE)
  {
    Lcd_Ctx[Instance].ReloadEnable = 0U;
  }
  else if(HAL_LTDC_Reload (&hlcd_ltdc, ReloadType) != HAL_OK)
  {
    ret = BSP_ERROR_PERIPH_FAILURE;
  }
  else
  {
    Lcd_Ctx[Instance].ReloadEnable = 1U;
  }

  return ret;
}

/**
  * @brief  Sets an LCD Layer visible
  * @param  Instance    LCD Instance
  * @param  LayerIndex  Visible Layer
  * @param  State  New state of the specified layer
  *          This parameter can be one of the following values:
  *            @arg  ENABLE
  *            @arg  DISABLE
  * @retval BSP status
  */
int32_t BSP_LCD_SetLayerVisible(uint32_t Instance, uint32_t LayerIndex, FunctionalState State)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(State == ENABLE)
    {
      __HAL_LTDC_LAYER_ENABLE(&hlcd_ltdc, LayerIndex);
    }
    else
    {
      __HAL_LTDC_LAYER_DISABLE(&hlcd_ltdc, LayerIndex);
    }

    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hlcd_ltdc);
    }
  }

  return ret;
}

/**
  * @brief  Configures the transparency.
  * @param  Instance      LCD Instance
  * @param  LayerIndex    Layer foreground or background.
  * @param  Transparency  Transparency
  *           This parameter must be a number between Min_Data = 0x00 and Max_Data = 0xFF
  * @retval BSP status
  */
int32_t BSP_LCD_SetTransparency(uint32_t Instance, uint32_t LayerIndex, uint8_t Transparency)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      (void)HAL_LTDC_SetAlpha(&hlcd_ltdc, Transparency, LayerIndex);
    }
    else
    {
      (void)HAL_LTDC_SetAlpha_NoReload(&hlcd_ltdc, Transparency, LayerIndex);
    }
  }

  return ret;
}

/**
  * @brief  Sets an LCD layer frame buffer address.
  * @param  Instance    LCD Instance
  * @param  LayerIndex  Layer foreground or background
  * @param  Address     New LCD frame buffer value
  * @retval BSP status
  */
int32_t BSP_LCD_SetLayerAddress(uint32_t Instance, uint32_t LayerIndex, uint32_t Address)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      (void)HAL_LTDC_SetAddress(&hlcd_ltdc, Address, LayerIndex);
    }
    else
    {
      (void)HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, Address, LayerIndex);
    }
  }

  return ret;
}

/**
  * @brief  Sets display window.
  * @param  Instance    LCD Instance
  * @param  LayerIndex  Layer index
  * @param  Xpos   LCD X position
  * @param  Ypos   LCD Y position
  * @param  Width  LCD window width
  * @param  Height LCD window height
  * @retval BSP status
  */
int32_t BSP_LCD_SetLayerWindow(uint32_t Instance, uint16_t LayerIndex, uint16_t Xpos, uint16_t Ypos, uint16_t Width, uint16_t Height)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      /* Reconfigure the layer size  and position */
      (void)HAL_LTDC_SetWindowSize(&hlcd_ltdc, Width, Height, LayerIndex);
      (void)HAL_LTDC_SetWindowPosition(&hlcd_ltdc, Xpos, Ypos, LayerIndex);
    }
    else
    {
      /* Reconfigure the layer size and position */
      (void)HAL_LTDC_SetWindowSize_NoReload(&hlcd_ltdc, Width, Height, LayerIndex);
      (void)HAL_LTDC_SetWindowPosition_NoReload(&hlcd_ltdc, Xpos, Ypos, LayerIndex);
    }

    Lcd_Ctx[Instance].XSize = Width;
    Lcd_Ctx[Instance].YSize = Height;
  }

  return ret;
}

/**
  * @brief  Configures and sets the color keying.
  * @param  Instance    LCD Instance
  * @param  LayerIndex  Layer foreground or background
  * @param  Color       Color reference
  * @retval BSP status
  */
int32_t BSP_LCD_SetColorKeying(uint32_t Instance, uint32_t LayerIndex, uint32_t Color)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      /* Configure and Enable the color Keying for LCD Layer */
      (void)HAL_LTDC_ConfigColorKeying(&hlcd_ltdc, Color, LayerIndex);
      (void)HAL_LTDC_EnableColorKeying(&hlcd_ltdc, LayerIndex);
    }
    else
    {
      /* Configure and Enable the color Keying for LCD Layer */
      (void)HAL_LTDC_ConfigColorKeying_NoReload(&hlcd_ltdc, Color, LayerIndex);
      (void)HAL_LTDC_EnableColorKeying_NoReload(&hlcd_ltdc, LayerIndex);
    }
  }
  return ret;
}

/**
  * @brief  Disables the color keying.
  * @param  Instance    LCD Instance
  * @param  LayerIndex Layer foreground or background
  * @retval BSP status
  */
int32_t BSP_LCD_ResetColorKeying(uint32_t Instance, uint32_t LayerIndex)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Ctx[Instance].ReloadEnable == 1U)
    {
      /* Disable the color Keying for LCD Layer */
      (void)HAL_LTDC_DisableColorKeying(&hlcd_ltdc, LayerIndex);
    }
    else
    {
      /* Disable the color Keying for LCD Layer */
      (void)HAL_LTDC_DisableColorKeying_NoReload(&hlcd_ltdc, LayerIndex);
    }
  }

  return ret;
}

/**
  * @brief  Gets the LCD X size.
  * @param  Instance  LCD Instance
  * @param  XSize     LCD width
  * @retval BSP status
  */
int32_t BSP_LCD_GetXSize(uint32_t Instance, uint32_t *XSize)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else if(Lcd_Drv->GetXSize != NULL)
  {
    if(Lcd_Drv->GetXSize(Lcd_CompObj, XSize) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Gets the LCD Y size.
  * @param  Instance  LCD Instance
  * @param  YSize     LCD Height
  * @retval BSP status
  */
int32_t BSP_LCD_GetYSize(uint32_t Instance, uint32_t *YSize)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else if(Lcd_Drv->GetYSize != NULL)
  {
    if(Lcd_Drv->GetYSize(Lcd_CompObj, YSize) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Switch On the display.
  * @param  Instance    LCD Instance
  * @retval BSP status
  */
int32_t BSP_LCD_DisplayOn(uint32_t Instance)
{
  int32_t ret;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Drv->DisplayOn(Lcd_CompObj) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_NONE;
    }
  }

  return ret;
}

/**
  * @brief  Switch Off the display.
  * @param  Instance    LCD Instance
  * @retval BSP status
  */
int32_t BSP_LCD_DisplayOff(uint32_t Instance)
{
  int32_t ret;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Drv->DisplayOff(Lcd_CompObj) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_NONE;
    }
  }

  return ret;
}

/**
  * @brief  Set the brightness value
  * @param  Instance    LCD Instance
  * @param  Brightness [00: Min (black), 100 Max]
  * @retval BSP status
  */
int32_t BSP_LCD_SetBrightness(uint32_t Instance, uint32_t Brightness)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Drv->SetBrightness(Lcd_CompObj, Brightness) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Set the brightness value
  * @param  Instance    LCD Instance
  * @param  Brightness [00: Min (black), 100 Max]
  * @retval BSP status
  */
int32_t BSP_LCD_GetBrightness(uint32_t Instance, uint32_t *Brightness)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Instance >= LCD_INSTANCES_NBR)
  {
    ret = BSP_ERROR_WRONG_PARAM;
  }
  else
  {
    if(Lcd_Drv->GetBrightness(Lcd_CompObj, Brightness) != BSP_ERROR_NONE)
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Draws a bitmap picture loaded in the internal Flash in currently active layer.
  * @param  Instance LCD Instance
  * @param  Xpos Bmp X position in the LCD
  * @param  Ypos Bmp Y position in the LCD
  * @param  pBmp Pointer to Bmp picture address in the internal Flash.
  * @retval BSP status
  */
int32_t BSP_LCD_DrawBitmap(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint8_t *pBmp)
{
  int32_t ret = BSP_ERROR_NONE;
  uint32_t index, width, height, bit_pixel;
  uint32_t Address;
  uint32_t input_color_mode;
  uint8_t *pbmp;

  /* Get bitmap data address offset */
  index = (uint32_t)pBmp[10] + ((uint32_t)pBmp[11] << 8) + ((uint32_t)pBmp[12] << 16)  + ((uint32_t)pBmp[13] << 24);

  /* Read bitmap width */
  width = (uint32_t)pBmp[18] + ((uint32_t)pBmp[19] << 8) + ((uint32_t)pBmp[20] << 16)  + ((uint32_t)pBmp[21] << 24);

  /* Read bitmap height */
  height = (uint32_t)pBmp[22] + ((uint32_t)pBmp[23] << 8) + ((uint32_t)pBmp[24] << 16)  + ((uint32_t)pBmp[25] << 24);

  /* Read bit/pixel */
  bit_pixel = (uint32_t)pBmp[28] + ((uint32_t)pBmp[29] << 8);

  /* Set the address */
  Address = hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (((Lcd_Ctx[Instance].XSize*Ypos) + Xpos)*Lcd_Ctx[Instance].BppFactor);

  /* Get the layer pixel format */
  if ((bit_pixel/8U) == 4U)
  {
    input_color_mode = DMA2D_INPUT_ARGB8888;
  }
  else if ((bit_pixel/8U) == 2U)
  {
    input_color_mode = DMA2D_INPUT_RGB565;
  }
  else
  {
    input_color_mode = DMA2D_INPUT_RGB888;
  }

  /* Bypass the bitmap header */
  pbmp = pBmp + (index + (width * (height - 1U) * (bit_pixel/8U)));

  /* Convert picture to ARGB8888 pixel format */
  for(index=0; index < height; index++)
  {
    /* Pixel format conversion */
    LL_ConvertLineToRGB(Instance, (uint32_t *)pbmp, (uint32_t *)Address, width, input_color_mode);

    /* Increment the source and destination buffers */
    Address+=  (Lcd_Ctx[Instance].XSize * Lcd_Ctx[Instance].BppFactor);
    pbmp -= width*(bit_pixel/8U);
  }

  return ret;
}
/**
  * @brief  Draw a horizontal line on LCD.
  * @param  Instance LCD Instance.
  * @param  Xpos X position.
  * @param  Ypos Y position.
  * @param  pData Pointer to RGB line data
  * @param  Width Rectangle width.
  * @param  Height Rectangle Height.
  * @retval BSP status.
  */
int32_t BSP_LCD_FillRGBRect(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint8_t *pData, uint32_t Width, uint32_t Height)
{
    uint32_t i;

#if (USE_DMA2D_TO_FILL_RGB_RECT == 1)
  uint32_t  Xaddress;
  for(i = 0; i < Height; i++)
  {
    /* Get the line address */
    Xaddress = hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (Lcd_Ctx[Instance].BppFactor*(((Lcd_Ctx[Instance].XSize + i)*Ypos) + Xpos));

    /* Write line */
    if(Lcd_Ctx[Instance].PixelFormat == LCD_PIXEL_FORMAT_RGB565)
    {
      LL_ConvertLineToRGB(Instance, (uint32_t *)pData, (uint32_t *)Xaddress, Width, DMA2D_INPUT_RGB565);
    }
    else
    {
      LL_ConvertLineToRGB(Instance, (uint32_t *)pData, (uint32_t *)Xaddress, Width, DMA2D_INPUT_ARGB8888);
    }
    pData += Lcd_Ctx[Instance].BppFactor*Width;
  }
#else
  uint32_t color, j;
  for(i = 0; i < Height; i++)
  {
    for(j = 0; j < Width; j++)
    {
      color = *pData | (*(pData + 1) << 8) | (*(pData + 2) << 16) | (*(pData + 3) << 24);
      BSP_LCD_WritePixel(Instance, Xpos + j, Ypos + i, color);
      pData += Lcd_Ctx[Instance].BppFactor;
    }
  }
#endif
  return BSP_ERROR_NONE;
}

/**
  * @brief  Draws an horizontal line in currently active layer.
  * @param  Instance   LCD Instance
  * @param  Xpos  X position
  * @param  Ypos  Y position
  * @param  Length  Line length
  * @param  Color Pixel color
  * @retval BSP status.
  */
int32_t BSP_LCD_DrawHLine(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint32_t Length, uint32_t Color)
{
  uint32_t  Xaddress;

  /* Get the line address */
  Xaddress = hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (Lcd_Ctx[Instance].BppFactor*((Lcd_Ctx[Instance].XSize*Ypos) + Xpos));

  /* Write line */
  if((Xpos + Length) > Lcd_Ctx[Instance].XSize)
  {
    Length = Lcd_Ctx[Instance].XSize - Xpos;
  }
  LL_FillBuffer(Instance, (uint32_t *)Xaddress, Length, 1, 0, Color);

  return BSP_ERROR_NONE;
}

/**
  * @brief  Draws a vertical line in currently active layer.
  * @param  Instance   LCD Instance
  * @param  Xpos  X position
  * @param  Ypos  Y position
  * @param  Length  Line length
  * @param  Color Pixel color
  * @retval BSP status.
  */
int32_t BSP_LCD_DrawVLine(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint32_t Length, uint32_t Color)
{
  uint32_t  Xaddress;

  /* Get the line address */
  Xaddress = (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress) + (Lcd_Ctx[Instance].BppFactor*(Lcd_Ctx[Instance].XSize*Ypos + Xpos));

  /* Write line */
  if((Ypos + Length) > Lcd_Ctx[Instance].YSize)
  {
    Length = Lcd_Ctx[Instance].YSize - Ypos;
  }
 LL_FillBuffer(Instance, (uint32_t *)Xaddress, 1, Length, (Lcd_Ctx[Instance].XSize - 1U), Color);

  return BSP_ERROR_NONE;
}

/**
  * @brief  Draws a full rectangle in currently active layer.
  * @param  Instance   LCD Instance
  * @param  Xpos X position
  * @param  Ypos Y position
  * @param  Width Rectangle width
  * @param  Height Rectangle height
  * @param  Color Pixel color
  * @retval BSP status.
  */
int32_t BSP_LCD_FillRect(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint32_t Width, uint32_t Height, uint32_t Color)
{
  uint32_t  Xaddress;

  /* Get the rectangle start address */
  Xaddress = (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress) + (Lcd_Ctx[Instance].BppFactor*(Lcd_Ctx[Instance].XSize*Ypos + Xpos));

  /* Fill the rectangle */
 LL_FillBuffer(Instance, (uint32_t *)Xaddress, Width, Height, (Lcd_Ctx[Instance].XSize - Width), Color);

  return BSP_ERROR_NONE;
}

/**
  * @brief  Reads an LCD pixel.
  * @param  Instance    LCD Instance
  * @param  Xpos X position
  * @param  Ypos Y position
  * @param  Color RGB pixel color
  * @retval BSP status
  */
int32_t BSP_LCD_ReadPixel(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint32_t *Color)
{
  if(hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].PixelFormat == LTDC_PIXEL_FORMAT_ARGB8888)
  {
    /* Read data value from SDRAM memory */
    *Color = *(__IO uint32_t*) (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (4U*(Ypos*Lcd_Ctx[Instance].XSize + Xpos)));
  }
  else /* if((hlcd_ltdc.LayerCfg[layer].PixelFormat == LTDC_PIXEL_FORMAT_RGB565) */
  {
    /* Read data value from SDRAM memory */
    *Color = *(__IO uint16_t*) (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (2U*(Ypos*Lcd_Ctx[Instance].XSize + Xpos)));
  }

  return BSP_ERROR_NONE;
}

/**
  * @brief  Draws a pixel on LCD.
  * @param  Instance    LCD Instance
  * @param  Xpos X position
  * @param  Ypos Y position
  * @param  Color Pixel color
  * @retval BSP status
  */
int32_t BSP_LCD_WritePixel(uint32_t Instance, uint32_t Xpos, uint32_t Ypos, uint32_t Color)
{
  if(hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].PixelFormat == LTDC_PIXEL_FORMAT_ARGB8888)
  {
    /* Write data value to SDRAM memory */
    *(__IO uint32_t*) (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (4U*(Ypos*Lcd_Ctx[Instance].XSize + Xpos))) = Color;
  }
  else
  {
    /* Write data value to SDRAM memory */
    *(__IO uint16_t*) (hlcd_ltdc.LayerCfg[Lcd_Ctx[Instance].ActiveLayer].FBStartAdress + (2U*(Ypos*Lcd_Ctx[Instance].XSize + Xpos))) = Color;
  }

  return BSP_ERROR_NONE;
}

/**
  * @}
  */

/** @defgroup STM32H747I_DISCO_LCD_Private_Functions Private Functions
  * @{
  */
/**
  * @brief  Fills a buffer.
  * @param  Instance LCD Instance
  * @param  pDst Pointer to destination buffer
  * @param  xSize Buffer width
  * @param  ySize Buffer height
  * @param  OffLine Offset
  * @param  Color Color index
  */
static void LL_FillBuffer(uint32_t Instance, uint32_t *pDst, uint32_t xSize, uint32_t ySize, uint32_t OffLine, uint32_t Color)
{
  uint32_t output_color_mode, input_color = Color;

  switch(Lcd_Ctx[Instance].PixelFormat)
  {
  case LCD_PIXEL_FORMAT_RGB565:
    output_color_mode = DMA2D_OUTPUT_RGB565; /* RGB565 */
    input_color = CONVERTRGB5652ARGB8888(Color);
    break;
  case LCD_PIXEL_FORMAT_RGB888:
  default:
    output_color_mode = DMA2D_OUTPUT_ARGB8888; /* ARGB8888 */
    break;
  }

  /* Register to memory mode with ARGB8888 as color Mode */
  hlcd_dma2d.Init.Mode         = DMA2D_R2M;
  hlcd_dma2d.Init.ColorMode    = output_color_mode;
  hlcd_dma2d.Init.OutputOffset = OffLine;

  hlcd_dma2d.Instance = DMA2D;

  /* DMA2D Initialization */
  if(HAL_DMA2D_Init(&hlcd_dma2d) == HAL_OK)
  {
    if(HAL_DMA2D_ConfigLayer(&hlcd_dma2d, 1) == HAL_OK)
    {
      if (HAL_DMA2D_Start(&hlcd_dma2d, input_color, (uint32_t)pDst, xSize, ySize) == HAL_OK)
      {
        /* Polling For DMA transfer */
        (void)HAL_DMA2D_PollForTransfer(&hlcd_dma2d, 25);
      }
    }
  }
}

/**
  * @brief  Converts a line to an RGB pixel format.
  * @param  Instance LCD Instance
  * @param  pSrc Pointer to source buffer
  * @param  pDst Output color
  * @param  xSize Buffer width
  * @param  ColorMode Input color mode
  */
static void LL_ConvertLineToRGB(uint32_t Instance, uint32_t *pSrc, uint32_t *pDst, uint32_t xSize, uint32_t ColorMode)
{
  uint32_t output_color_mode;

  switch(Lcd_Ctx[Instance].PixelFormat)
  {
  case LCD_PIXEL_FORMAT_RGB565:
    output_color_mode = DMA2D_OUTPUT_RGB565; /* RGB565 */
    break;
  case LCD_PIXEL_FORMAT_RGB888:
  default:
    output_color_mode = DMA2D_OUTPUT_ARGB8888; /* ARGB8888 */
    break;
  }

  /* Configure the DMA2D Mode, Color Mode and output offset */
  hlcd_dma2d.Init.Mode         = DMA2D_M2M_PFC;
  hlcd_dma2d.Init.ColorMode    = output_color_mode;
  hlcd_dma2d.Init.OutputOffset = 0;

  /* Foreground Configuration */
  hlcd_dma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hlcd_dma2d.LayerCfg[1].InputAlpha = 0xFF;
  hlcd_dma2d.LayerCfg[1].InputColorMode = ColorMode;
  hlcd_dma2d.LayerCfg[1].InputOffset = 0;

  hlcd_dma2d.Instance = DMA2D;

  /* DMA2D Initialization */
  if(HAL_DMA2D_Init(&hlcd_dma2d) == HAL_OK)
  {
    if(HAL_DMA2D_ConfigLayer(&hlcd_dma2d, 1) == HAL_OK)
    {
      if (HAL_DMA2D_Start(&hlcd_dma2d, (uint32_t)pSrc, (uint32_t)pDst, xSize, 1) == HAL_OK)
      {
        /* Polling For DMA transfer */
        (void)HAL_DMA2D_PollForTransfer(&hlcd_dma2d, 50);
      }
    }
  }
}

/*******************************************************************************
                       BSP Routines:
                                       LTDC
                                       DMA2D
                                       DSI
*******************************************************************************/
/**
  * @brief  Initialize the BSP LTDC Msp.
  * @param  hltdc  LTDC handle
  * @retval None
  */
static void LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
  if(hltdc->Instance == LTDC)
  {
    /** Enable the LTDC clock */
    __HAL_RCC_LTDC_CLK_ENABLE();


    /** Toggle Sw reset of LTDC IP */
    __HAL_RCC_LTDC_FORCE_RESET();
    __HAL_RCC_LTDC_RELEASE_RESET();
  }
}

/**
  * @brief  De-Initializes the BSP LTDC Msp
  * @param  hltdc  LTDC handle
  * @retval None
  */
static void LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc)
{
  if(hltdc->Instance == LTDC)
  {
    /** Force and let in reset state LTDC */
    __HAL_RCC_LTDC_FORCE_RESET();

    /** Disable the LTDC */
    __HAL_RCC_LTDC_CLK_DISABLE();
  }
}

/**
  * @brief  Initialize the BSP DMA2D Msp.
  * @param  hdma2d  DMA2D handle
  * @retval None
  */
static void DMA2D_MspInit(DMA2D_HandleTypeDef *hdma2d)
{
  if(hdma2d->Instance == DMA2D)
  {
    /** Enable the DMA2D clock */
    __HAL_RCC_DMA2D_CLK_ENABLE();

    /** Toggle Sw reset of DMA2D IP */
    __HAL_RCC_DMA2D_FORCE_RESET();
    __HAL_RCC_DMA2D_RELEASE_RESET();
  }
}

/**
  * @brief  De-Initializes the BSP DMA2D Msp
  * @param  hdma2d  DMA2D handle
  * @retval None
  */
static void DMA2D_MspDeInit(DMA2D_HandleTypeDef *hdma2d)
{
  if(hdma2d->Instance == DMA2D)
  {
    /** Disable IRQ of DMA2D IP */
    HAL_NVIC_DisableIRQ(DMA2D_IRQn);

    /** Force and let in reset state DMA2D */
    __HAL_RCC_DMA2D_FORCE_RESET();

    /** Disable the DMA2D */
    __HAL_RCC_DMA2D_CLK_DISABLE();
  }
}

/**
  * @brief  Initialize the BSP DSI Msp.
  * @param  hdsi  DSI handle
  * @retval None
  */
static void DSI_MspInit(DSI_HandleTypeDef *hdsi)
{
  if(hdsi->Instance == DSI)
  {
    /** Enable DSI Host and wrapper clocks */
    __HAL_RCC_DSI_CLK_ENABLE();

    /** Soft Reset the DSI Host and wrapper */
    __HAL_RCC_DSI_FORCE_RESET();
    __HAL_RCC_DSI_RELEASE_RESET();
  }
}

/**
  * @brief  De-Initializes the BSP DSI Msp
  * @param  hdsi  DSI handle
  * @retval None
  */
static void DSI_MspDeInit(DSI_HandleTypeDef *hdsi)
{
  if(hdsi->Instance == DSI)
  {
    /** Disable IRQ of DSI IP */
    HAL_NVIC_DisableIRQ(DSI_IRQn);

    /** Force and let in reset state the DSI Host + Wrapper IPs */
    __HAL_RCC_DSI_FORCE_RESET();

    /** Disable the DSI Host and Wrapper clocks */
    __HAL_RCC_DSI_CLK_DISABLE();
  }
}

/**
  * @brief  DCS or Generic short/long write command
  * @param  ChannelNbr Virtual channel ID
  * @param  Reg Register to be written
  * @param  pData pointer to a buffer of data to be write
  * @param  Size To precise command to be used (short or long)
  * @retval BSP status
  */
static int32_t DSI_IO_Write(uint16_t ChannelNbr, uint16_t Reg, uint8_t *pData, uint16_t Size)
{
  int32_t ret = BSP_ERROR_NONE;

  if(Size <= 1U)
  {
    if(HAL_DSI_ShortWrite(&hlcd_dsi, ChannelNbr, DSI_DCS_SHORT_PKT_WRITE_P1, Reg, (uint32_t)pData[Size]) != HAL_OK)
    {
      ret = BSP_ERROR_BUS_FAILURE;
    }
  }
  else
  {
    if(HAL_DSI_LongWrite(&hlcd_dsi, ChannelNbr, DSI_DCS_LONG_PKT_WRITE, Size, (uint32_t)Reg, pData) != HAL_OK)
    {
      ret = BSP_ERROR_BUS_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  DCS or Generic read command
  * @param  ChannelNbr Virtual channel ID
  * @param  Reg Register to be read
  * @param  pData pointer to a buffer to store the payload of a read back operation.
  * @param  Size  Data size to be read (in byte).
  * @retval BSP status
  */
static int32_t DSI_IO_Read(uint16_t ChannelNbr, uint16_t Reg, uint8_t *pData, uint16_t Size)
{
  int32_t ret = BSP_ERROR_NONE;

  if(HAL_DSI_Read(&hlcd_dsi, ChannelNbr, pData, Size, DSI_DCS_SHORT_PKT_READ, Reg, pData) != HAL_OK)
  {
    ret = BSP_ERROR_BUS_FAILURE;
  }

  return ret;
}

#if (USE_LCD_CTRL_ADV7533 > 0)
/**
  * @brief  Get HDMI DSI video mode timings for selected format
  * @param  Format   Display format
  * @param  Timing   pointer to timing structure
  * @retval None
  */
static void LCD_Get_HDMI_VideoModeTiming(uint32_t Format, LCD_HDMI_Timing_t *Timing)
{
  if(Format == HDMI_FORMAT_720_480)
  {
    Timing->HSYNC = 62U;
    Timing->HBP   = 60U;
    Timing->HFP   = 30U;
    Timing->VSYNC = 6U;
    Timing->VBP   = 19U;
    Timing->VFP   = 9U;
    Timing->HACT  = 720U;
    Timing->VACT  = 480U;
  }
  else /* (Format == HDMI_FORMAT_720_576) */
  {
    Timing->HSYNC = 64U;
    Timing->HBP   = 68U;
    Timing->HFP   = 12U;
    Timing->VSYNC = 5U;
    Timing->VBP   = 39U;
    Timing->VFP   = 5U;
    Timing->HACT  = 720U;
    Timing->VACT  = 576U;
  }
  Timing->ClockLaneHS2LPTime = 0x14;
  Timing->ClockLaneLP2HSTime = 0x14;
  Timing->DataLaneHS2LPTime = 0x0A;
  Timing->DataLaneLP2HSTime = 0x0A;
  Timing->DataLaneMaxReadTime = 0x00;
  Timing->StopWaitTime = 0x00;
}

/**
  * @brief  LTDC initialization
  * @param  Timing   pointer to timing structure
  * @retval None
  */
void LTDC_HDMI_Init(LCD_HDMI_Timing_t *Timing)
{
  /* Background value */
  hlcd_ltdc.Init.Backcolor.Blue = 0x00;
  hlcd_ltdc.Init.Backcolor.Green = 0xFF;
  hlcd_ltdc.Init.Backcolor.Red = 0xFF;

  hlcd_ltdc.Init.HorizontalSync     = Timing->HSYNC - 1;
  hlcd_ltdc.Init.AccumulatedHBP     = Timing->HSYNC + Timing->HBP - 1;
  hlcd_ltdc.Init.AccumulatedActiveW = Timing->HSYNC + Timing->HACT + Timing->HBP - 1;
  hlcd_ltdc.Init.TotalWidth         = Timing->HSYNC + Timing->HACT + Timing->HBP + Timing->HFP - 1;
  hlcd_ltdc.Init.VerticalSync       = Timing->VSYNC - 1;
  hlcd_ltdc.Init.AccumulatedVBP     = Timing->VSYNC + Timing->VBP - 1;
  hlcd_ltdc.Init.AccumulatedActiveH = Timing->VSYNC + Timing->VACT + Timing->VBP - 1;
  hlcd_ltdc.Init.TotalHeigh         = Timing->VSYNC + Timing->VACT + Timing->VBP + Timing->VFP - 1;

  /* Polarity */
  hlcd_ltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hlcd_ltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hlcd_ltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hlcd_ltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hlcd_ltdc.Instance = LTDC;

  /* Initialize the LTDC */
  (void)HAL_LTDC_Init(&hlcd_ltdc);
}

/**
  * @brief  Register Bus IOs if component ID is OK
  * @retval error status
  */
static int32_t ADV7533_Probe(void)
{
  int32_t ret;
  uint32_t id;
  ADV7533_IO_t              IOCtx;
  static ADV7533_Object_t         ADV7533Obj;

  /* Configure the audio driver */
  IOCtx.Address     = ADV7533_MAIN_I2C_ADDR;
  IOCtx.Init        = BSP_I2C4_Init;
  IOCtx.DeInit      = BSP_I2C4_DeInit;
  IOCtx.ReadReg     = BSP_I2C4_ReadReg;
  IOCtx.WriteReg    = BSP_I2C4_WriteReg;
  IOCtx.GetTick     = BSP_GetTick;

  if(ADV7533_RegisterBusIO (&ADV7533Obj, &IOCtx) != ADV7533_OK)
  {
    ret = BSP_ERROR_BUS_FAILURE;
  }
  else if(ADV7533_ReadID(&ADV7533Obj, &id) != ADV7533_OK)
  {
    ret = BSP_ERROR_COMPONENT_FAILURE;
  }
  else if(id != ADV7533_ID)
  {
    ret = BSP_ERROR_UNKNOWN_COMPONENT;
  }
  else
  {
    Lcd_Drv = (LCD_Drv_t *) &ADV7533_LCD_Driver;
    Lcd_CompObj = &ADV7533Obj;
    if(Lcd_Drv->Init(Lcd_CompObj, 0, 0) != ADV7533_OK)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
    else if(ADV7533_Configure((ADV7533_Object_t *)Lcd_CompObj, 2)!= ADV7533_OK)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_NONE;
    }
  }

  return ret;
}
#endif /*USE_LCD_CTRL_ADV7533 */
#if (USE_LCD_CTRL_OTM8009A > 0)
/**
  * @brief  Register Bus IOs if component ID is OK
  * @retval error status
  */
static int32_t OTM8009A_Probe(uint32_t ColorCoding, uint32_t Orientation)
{
  int32_t ret;
  uint32_t id;
  OTM8009A_IO_t              IOCtx;
  static OTM8009A_Object_t   OTM8009AObj;

  /* Configure the audio driver */
  IOCtx.Address     = 0;
  IOCtx.GetTick     = BSP_GetTick;
  IOCtx.WriteReg    = DSI_IO_Write;
  IOCtx.ReadReg     = DSI_IO_Read;

  if(OTM8009A_RegisterBusIO(&OTM8009AObj, &IOCtx) != OTM8009A_OK)
  {
    ret = BSP_ERROR_BUS_FAILURE;
  }
  else
  {
    Lcd_CompObj = &OTM8009AObj;

    if(OTM8009A_ReadID((OTM8009A_Object_t *)Lcd_CompObj, &id) != OTM8009A_OK)
    {
      ret = BSP_ERROR_COMPONENT_FAILURE;
    }

    else
    {
      Lcd_Drv = (LCD_Drv_t *)(void *) &OTM8009A_LCD_Driver;
      if(Lcd_Drv->Init(Lcd_CompObj, ColorCoding, Orientation) != OTM8009A_OK)
      {
        ret = BSP_ERROR_COMPONENT_FAILURE;
      }
      else
      {
        ret = BSP_ERROR_NONE;
      }
    }
  }
  return ret;
}
#endif /* USE_LCD_CTRL_OTM8009A */
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
