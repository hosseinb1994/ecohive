/******************************************************************************
 * @file           : SD_Log.h
 * @brief          : Application-facing microSD CSV logging API
 * @author         : Hossein Baghaei
 * @date           : 22-Jul-2026
 * @version        : 1.0
 *
 * @details
 * Sits on top of FatFs + SD_SPI.c and mirrors the exact same CSV rows
 * CSV_Log_Task already prints over UART into a file on the microSD card, so
 * you get a 2-day-unattended backup of the same data with no laptop needed.
 * See PAPER_CHANGES.md (Part 3) for the wiring table, clock speeds, and the
 * reasoning behind every design choice below.
 ******************************************************************************/
#ifndef INC_SD_LOG_H_
#define INC_SD_LOG_H_

#include <stdint.h>

/**
 * Master on/off switch for the entire SD-card logging feature. Set to 0 to
 * fully compile it out: with this at 0, SD_Log.c never references FatFs,
 * diskio.c or SD_SPI.c, so the linker's --gc-sections (already enabled in
 * this project) drops all three from the final binary. Flip this to 0 when
 * you need clean DWT cycle-counter timing for model inference and don't
 * want background SD writes adding latency noise.
 */
//#define ENABLE_SD_LOGGING 1
#define ENABLE_SD_LOGGING 0

/**
 * f_sync() is called after this many successfully written data rows, so a
 * power loss can cost at most this many rows of data - never the file
 * itself (FAT metadata is flushed at every sync too).
 */
#define SD_SYNC_EVERY_N_ROWS 10

typedef enum {
    SD_LOG_DISABLED = 0, /* ENABLE_SD_LOGGING is 0                        */
    SD_LOG_OK,           /* mounted, file open, writing normally          */
    SD_LOG_FAILED        /* mount/open/write error - see LED note below   */
} SD_LogStatus;

/**
 * @brief Mounts the card, creates /ECOHIVE if it doesn't exist yet, and
 *        opens the next auto-incremented /ECOHIVE/LOG_XXXX.CSV file
 *        (XXXX = 0001, 0002, ... - the lowest number not already on the
 *        card), writing the given header line into it once.
 * @note  Call once at startup. On failure, status becomes SD_LOG_FAILED and
 *        SD_Log_WriteRow() will keep retrying on its own - no need to call
 *        this again yourself.
 */
void SD_Log_Init(const char *header_line, uint16_t header_len);

/**
 * @brief Appends one CSV row (the same bytes sent over UART) to the open
 *        log file, syncing to the card every SD_SYNC_EVERY_N_ROWS rows.
 * @note  Never blocks indefinitely and never crashes on an SD fault - on
 *        any mount/write/sync error this marks status SD_LOG_FAILED and
 *        returns; later calls retry the mount on a backoff so a temporarily
 *        missing/failed card can't stall the measurement task and logging
 *        resumes automatically if the card comes back.
 */
void SD_Log_WriteRow(const char *row, uint16_t row_len);

/**
 * @brief Current SD status - drives the heartbeat LED's blink pattern
 *        (steady heartbeat = OK/disabled, fast blink = FAILED).
 */
SD_LogStatus SD_Log_GetStatus(void);

#endif /* INC_SD_LOG_H_ */
