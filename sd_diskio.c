/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @author  MCD Application Team
  * @brief   SD Disk I/O driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "stm32f769i_discovery_audio.h"

#define SD_MODE_DMA 0
#define SD_UNALIGNED_WA 1

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* use the default SD timout as defined in the platform BSP driver*/
#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT 30 * 1000
#endif

#define SD_DEFAULT_BLOCK_SIZE 512

/*
 * Depending on the usecase, the SD card initialization could be done at the
 * application level, if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize().
 */

/* #define DISABLE_SD_INIT */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

#if SD_MODE_DMA
int8_t sd_txrx_complete = 1;

int is_sd_txrx_operation ()
{
    return sd_txrx_complete ? 0 : 1;
}

static void sd_rxtx_wait ()
{
    while (!sd_txrx_complete) {}
}
#endif /*SD_MODE_DMA*/

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
  DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif  /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if  _USE_WRITE == 1
  SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
  SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;
  if(BSP_SD_GetCardState() == MSD_OK)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
  Stat = STA_NOINIT;
#if !defined(DISABLE_SD_INIT)

  if(BSP_SD_Init() == MSD_OK)
  {
    Stat = SD_CheckStatus(lun);
  }

#else
  Stat = SD_CheckStatus(lun);
#endif
  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
    return SD_CheckStatus(lun);
}

#if SD_UNALIGNED_WA

#if (_MIN_SS != _MAX_SS)
#error "Unsupported mode"
#endif

static uint8_t sd_local_buf[_MAX_SS];

DRESULT SD_Uread(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    DWORD end = sector + count;

    for (; sector < end; sector++, buff += _MAX_SS) {
        res = BSP_SD_ReadBlocks((uint32_t *)sd_local_buf,
                                (uint32_t)sector,
                                1,
                                SD_TIMEOUT);
        if (res != MSD_OK) {
            return RES_ERROR;
        }
        memcpy(buff, sd_local_buf, _MAX_SS);
    }
    return RES_OK;
}

#endif /*SD_UNALIGNED_WA*/

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  BSP_LED_On(LED2);
#if SD_MODE_DMA
  sd_rxtx_wait();
  sd_txrx_complete = 0;
  if(BSP_SD_ReadBlocks_DMA((uint32_t*)buff,
                            (uint32_t)sector,
                            count) == MSD_OK)
  {
    sd_rxtx_wait();
    res = RES_OK;
  }
#elif SD_UNALIGNED_WA
  int unaligned = ((uint32_t)buff) & 0x3;
  if (unaligned) {
     res = SD_Uread(lun, buff, sector, count);
  } else {
     res = BSP_SD_ReadBlocks((uint32_t *)buff,
                          (uint32_t)sector,
                          count,
                          SD_TIMEOUT);
     res = (res == MSD_OK) ? RES_OK : RES_ERROR;
  }
#else
  res = BSP_SD_ReadBlocks((uint32_t *)buff,
                          (uint32_t)sector,
                          count,
                          SD_TIMEOUT);
  res = (res == MSD_OK) ? RES_OK : RES_ERROR;
#endif /*SD_MODE_DMA*/
  BSP_LED_Off(LED2);
  return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  BSP_LED_On(LED2);

#if SD_MODE_DMA
  sd_rxtx_wait();
  sd_txrx_complete = 0;
  if(BSP_SD_WriteBlocks_DMA((uint32_t*)buff,
                            (uint32_t)sector,
                            count) == MSD_OK)
  {
    sd_rxtx_wait();
    res = RES_OK;
  }
#else /*SD_MODE_DMA*/
  res = BSP_SD_WriteBlocks((uint32_t*)buff,
                           (uint32_t)sector,
                           count,
                           SD_TIMEOUT);

  res = (res == MSD_OK) ? RES_OK : RES_ERROR;
#endif /*SD_MODE_DMA*/
  BSP_LED_Off(LED2);
  return res;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(WORD*)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
	res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */

/**
  * @brief BSP SD Abort callbacks
  * @retval None
  */
void BSP_SD_AbortCallback(void)
{

}

/**
  * @brief BSP Tx Transfer completed callbacks
  * @retval None
  */
void BSP_SD_WriteCpltCallback(void)
{
#if SD_MODE_DMA
    sd_txrx_complete = 1;
#endif
}

/**
  * @brief BSP Rx Transfer completed callbacks
  * @retval None
  */
void BSP_SD_ReadCpltCallback(void)
{
#if SD_MODE_DMA
    sd_txrx_complete = 2;
#endif
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

