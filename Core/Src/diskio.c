/******************************************************************************
 * @file           : diskio.c
 * @brief          : FatFs low-level disk I/O glue, backed by SD_SPI.c
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * This is the project-specific glue FatFs expects (see diskio.h, vendored
 * from ChaN's FatFs via the local STM32Cube_FW_F4 package) - it is NOT
 * vendored code, it is written for this project to bind FatFs to our own
 * bare-metal SD_SPI.c driver instead of ST's HAL-based SD/BSP drivers.
 * Single physical drive (pdrv is always 0, matching _VOLUMES==1).
 ******************************************************************************/
#include "diskio.h"
#include "SD_SPI.h"
#include <string.h>

static volatile DSTATUS s_status = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    s_status = SD_SPI_Init() ? 0 : STA_NOINIT;
    return s_status;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return s_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || s_status & STA_NOINIT) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++)
    {
        if (!SD_SPI_ReadBlock(sector + i, buff + (i * 512U)))
        {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || s_status & STA_NOINIT) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++)
    {
        if (!SD_SPI_WriteBlock(sector + i, buff + (i * 512U)))
        {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    if (s_status & STA_NOINIT) return RES_NOTRDY;

    switch (cmd)
    {
        case CTRL_SYNC:
            // SD_SPI_WriteBlock() already waits out the card's internal
            // program-busy period before returning, so there is nothing
            // buffered on our side left to flush.
            return RES_OK;

        default:
            // GET_SECTOR_COUNT/GET_SECTOR_SIZE/GET_BLOCK_SIZE are only
            // needed when _USE_MKFS==1 or _MAX_SS!=_MIN_SS, neither of
            // which apply here (see ffconf.h).
            return RES_PARERR;
    }
}

/**
 * @brief FAT directory-entry timestamp source.
 * @note  _FS_NORTC is set to 1 in ffconf.h (this board has no RTC and the
 *        real per-row time already lives in the CSV's timestamp_ms column),
 *        so FatFs never actually calls this - it uses the fixed
 *        _NORTC_MON/_NORTC_MDAY/_NORTC_YEAR date instead. Defined anyway so
 *        the build doesn't depend on that config staying exactly as-is.
 */
DWORD get_fattime(void)
{
    // 2026-07-22 00:00:00, FAT's packed date/time format.
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)7 << 21) | ((DWORD)22 << 16);
}
