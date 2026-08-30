/******************************************************************************
 * @file           : SD_Log.c
 * @brief          : Application-facing microSD CSV logging implementation
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * See SD_Log.h for the API contract. Everything below is guarded by
 * ENABLE_SD_LOGGING - when it's 0, only the trivial disabled-stub bottom
 * half of this file is compiled, which never touches FatFs/diskio/SD_SPI,
 * so the linker's --gc-sections drops all of that code from the binary.
 ******************************************************************************/
#include "SD_Log.h"

#if ENABLE_SD_LOGGING

#include "ff.h"
#include "SD_SPI.h"
#include "UART.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* How many consecutive failed SD_Log_WriteRow() calls to skip before trying
 * to remount, once the card has failed. At CSV_LOG_INTERVAL_MS (~7s/row)
 * this spaces retries out to roughly every 35s instead of hammering a
 * missing/dead card every single row (each full mount attempt is bounded
 * but can take up to ~2s worst case - see SD_SPI.c). */
#define SD_REMOUNT_BACKOFF_ROWS 5

extern SemaphoreHandle_t xUARTMutex;

static FATFS s_fatfs;
static FIL   s_file;
static volatile SD_LogStatus s_status = SD_LOG_FAILED;
static bool  s_file_open = false;
static uint32_t s_rows_since_sync = 0;
static uint32_t s_failed_call_count = 0;

static char     s_header_copy[96];
static uint16_t s_header_len = 0;

/** Every debug line here is "#"-prefixed so it's filterable like the rest of
 *  the project's non-CSV UART output, and goes through xUARTMutex so it
 *  can't interleave with a CSV row or another task's debug line. */
static void sd_debug_print(const char *msg, int len)
{
    if (xSemaphoreTake(xUARTMutex, (TickType_t)50) == pdTRUE)
    {
        Print_Message((void *)msg, (uint16_t)len);
        xSemaphoreGive(xUARTMutex);
    }
}

/** Dumps exactly how far the SD card's power-up handshake got, straight
 *  from SD_SPI.c's step-by-step record - this is what tells you whether the
 *  problem is "card never responded at all" (wiring/power/level shifter/CS)
 *  vs "card responded but wouldn't leave idle" (a much rarer, usually
 *  actually-dead-card symptom). */
static void sd_debug_print_init_diag(void)
{
    SD_InitDiag diag = SD_SPI_GetInitDiag();
    char buf[150];
    int len = snprintf(buf, sizeof(buf),
        "# SD: init detail - CMD0 r1=0x%02X (%s) | CMD8/SDv2=%s | ACMD41=%s | CMD1(MMC)=%s | CMD58=%s\r\n",
        diag.cmd0_r1,
        diag.cmd0_ok ? "ok" :
            (diag.cmd0_r1 == 0xFF ? "NO RESPONSE-check power/CS/MISO/level shifter" : "rejected/garbled"),
        diag.cmd8_ok ? "yes" : "no",
        diag.acmd41_ok ? "ok" : "failed/timeout",
        diag.cmd1_ok ? "ok" : "failed/not-tried",
        diag.cmd58_ok ? "ok" : "failed/not-tried");
    sd_debug_print(buf, len);
}

/** Finds the lowest-numbered /ECOHIVE/LOG_XXXX.CSV that doesn't exist yet
 *  and creates it, so a restart never overwrites previous data. */
static FRESULT find_next_index_and_create(void)
{
    char path[32];
    FILINFO fno;
    uint16_t index = 1;

    for (; index <= 9999; index++)
    {
        snprintf(path, sizeof(path), "/ECOHIVE/LOG_%04u.CSV", index);
        if (f_stat(path, &fno) == FR_NO_FILE) break; // first free name
    }
    if (index > 9999) index = 9999; // pathological fallback, won't happen in practice

    snprintf(path, sizeof(path), "/ECOHIVE/LOG_%04u.CSV", index);
    return f_open(&s_file, path, FA_CREATE_ALWAYS | FA_WRITE);
}

static bool try_mount_and_open(void)
{
    char buf[150];
    int len;
    FRESULT fr;

    // opt=1: mount right now (not lazily) so a missing/dead card is
    // detected immediately rather than on the first later file access.
    fr = f_mount(&s_fatfs, "", 1);
    if (fr != FR_OK)
    {
        if (fr == FR_NOT_READY)
        {
            len = snprintf(buf, sizeof(buf),
                "# SD: f_mount FAILED, FRESULT=%d NOT_READY (card never powered up over SPI)\r\n",
                (int)fr);
            sd_debug_print(buf, len);
            sd_debug_print_init_diag();
        }
        else if (fr == FR_NO_FILESYSTEM)
        {
            len = snprintf(buf, sizeof(buf),
                "# SD: f_mount FAILED, FRESULT=%d NO_FILESYSTEM (card responded, but no valid FAT/FAT32 volume - reformat the card as FAT32 on a PC)\r\n",
                (int)fr);
            sd_debug_print(buf, len);
        }
        else
        {
            len = snprintf(buf, sizeof(buf), "# SD: f_mount FAILED, FRESULT=%d\r\n", (int)fr);
            sd_debug_print(buf, len);
        }
        return false;
    }

    FRESULT mkdir_res = f_mkdir("/ECOHIVE");
    if (mkdir_res != FR_OK && mkdir_res != FR_EXIST)
    {
        len = snprintf(buf, sizeof(buf), "# SD: f_mkdir(/ECOHIVE) FAILED, FRESULT=%d\r\n", (int)mkdir_res);
        sd_debug_print(buf, len);
        return false;
    }

    fr = find_next_index_and_create();
    if (fr != FR_OK)
    {
        len = snprintf(buf, sizeof(buf), "# SD: could not create log file, FRESULT=%d\r\n", (int)fr);
        sd_debug_print(buf, len);
        return false;
    }

    UINT written = 0;
    fr = f_write(&s_file, s_header_copy, s_header_len, &written);
    if (fr != FR_OK || written != s_header_len)
    {
        len = snprintf(buf, sizeof(buf), "# SD: header write FAILED, FRESULT=%d, wrote=%u/%u bytes\r\n",
                       (int)fr, written, s_header_len);
        sd_debug_print(buf, len);
        f_close(&s_file);
        return false;
    }

    fr = f_sync(&s_file);
    if (fr != FR_OK)
    {
        len = snprintf(buf, sizeof(buf), "# SD: header sync FAILED, FRESULT=%d\r\n", (int)fr);
        sd_debug_print(buf, len);
        // Not treated as fatal here (matches prior behavior) - the file and
        // header are still on the card, just not guaranteed flushed yet.
    }

    len = snprintf(buf, sizeof(buf), "# SD: mounted OK, log file created, header written\r\n");
    sd_debug_print(buf, len);

    s_rows_since_sync = 0;
    return true;
}

void SD_Log_Init(const char *header_line, uint16_t header_len)
{
    if (header_len > sizeof(s_header_copy)) header_len = sizeof(s_header_copy);
    memcpy(s_header_copy, header_line, header_len);
    s_header_len = header_len;

    s_file_open = try_mount_and_open();
    s_status = s_file_open ? SD_LOG_OK : SD_LOG_FAILED;
    s_failed_call_count = 0;
}

void SD_Log_WriteRow(const char *row, uint16_t row_len)
{
    if (!s_file_open)
    {
        s_failed_call_count++;
        if (s_failed_call_count % SD_REMOUNT_BACKOFF_ROWS != 0)
        {
            s_status = SD_LOG_FAILED; // still down, but not retrying this call
            return;
        }
        if (!try_mount_and_open())
        {
            s_status = SD_LOG_FAILED;
            return;
        }
        s_file_open = true;
    }

    UINT written = 0;
    FRESULT fr = f_write(&s_file, row, row_len, &written);
    if (fr != FR_OK || written != row_len)
    {
        f_close(&s_file);
        s_file_open = false;
        s_status = SD_LOG_FAILED;
        return;
    }

    s_rows_since_sync++;
    if (s_rows_since_sync >= SD_SYNC_EVERY_N_ROWS)
    {
        if (f_sync(&s_file) != FR_OK)
        {
            f_close(&s_file);
            s_file_open = false;
            s_status = SD_LOG_FAILED;
            return;
        }
        s_rows_since_sync = 0;
    }

    s_status = SD_LOG_OK;
    s_failed_call_count = 0;
}

SD_LogStatus SD_Log_GetStatus(void)
{
    return s_status;
}

#else /* !ENABLE_SD_LOGGING - fully compiled out, see SD_Log.h */

void SD_Log_Init(const char *header_line, uint16_t header_len)
{
    (void)header_line;
    (void)header_len;
}

void SD_Log_WriteRow(const char *row, uint16_t row_len)
{
    (void)row;
    (void)row_len;
}

SD_LogStatus SD_Log_GetStatus(void)
{
    return SD_LOG_DISABLED;
}

#endif /* ENABLE_SD_LOGGING */
